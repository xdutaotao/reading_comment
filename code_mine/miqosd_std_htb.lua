#!/usr/bin/lua

local px =  require "posix"
local uci=  require 'luci.model.uci'
local util = require 'luci.util'
local io=   require 'io'
local socket= require 'socket'
local json= require 'json'
local fs = require "nixio.fs"
local ubus = require "ubus"

local g_debug=false
local g_outlog='/tmp/miqos.log'
local g_extend_flag=false       -- 实验证明，往上扩展带宽的QoS是不可行的，保持关闭!!!

local g_CONFIG_HZ=100  -- HZ用来计算buffer量大小

--logger
--[[1 Alert, 3 Error 7 Debug ]]--
px.openlog("miqos","np",LOG_USER)
function logger(loglevel,msg)
    px.syslog(loglevel,msg)
end

--read args
local QOS_VER='CTF'
if #arg == 1 and arg[1] == 'std' then
    logger(3, ' STD mode , arg: ' .. #arg)
    QOS_VER='STD'
else
    logger(3, ' CTF mode , arg: ' .. #arg)
    QOS_VER='CTF'
end

-------config status-----------
local cfg_default_group,cfg_default_mode
local cfg_changed = 'NONE'       -- cfg is changed by manual

local g_default_min_updown_factor=0.5
local const_total_max_grp_ratio=1.0
local g_quantum_value=1600
local g_idle_timeout=301     -- if not active in 301s, regard as dead
local g_idle_timeout_wl=5    -- for wireless-point, if less then 5, regard it as dead

local group_def={}
local mode_def={}
local class_def={}

local cfg_host='127.0.0.1'
local cfg_port='1035'

local UP,DOWN='UP','DOWN'

-------current status-----------
local cur_bandwidth={UP=0,DOWN=0}       -- tc 实际使用的band
local cfg_bandwidth={UP=0,DOWN=0}       -- 配置中读取的band
local rec_bandwidth={UP=0,DOWN=0}       -- 记录的系统最大band
-- local act_bandwidth={UP=0,DOWN=0}       -- 确定下来的band
local tc_bandwidth={UP=0,DOWN=0}        -- 当前确定的band
local band_extend_factor=1.05

local cfg_qos_check_interval=15     --check interval in seconds
local cur_qos_enabled, cfg_qos_enabled='0', '0'
local g_alive_hosts={}         --current host list

local g_crt_ipmac_map={} --维护当前tc的中的ip规则
local g_nxt_ipmac_map={} --维护下一次tc的ip规则,在规则生效后,会copy给crt_ipmac表维护

local QOS_TYPE='auto'      -- 默认是自动模式
-- local g_f_init={UP=true,DOWN=true,ipt=true}      -- 表示是否需要重建规则框架

local g_flow_type, g_num_of_clients = '', 0
local g_wan_if,g_lan_if
local g_wan_type,g_cur_wan_type = '',''
local g_ifb,g_ifb_status='ifb0',DOWN
local g_host_rate_factor=0.8
local g_htb_buffer_factor=1.5

--layer1,layer2_host,layer2_prio状态
local g_htb_layer1_ready=false
local g_prio_layer1_ready=false
local g_htb_layer2_host_ready=false

local g_cur_guest_max_band={UP=0,DOWN=0}
local g_guest_max_band={UP=0,DOWN=0 }
local g_guest_default_band_factor=0.6       -- 默认guest网络拥有60%总带宽
local g_xq_default_band_factor=0.8          -- 在prio模式下xq的带宽限制在80%总带宽

local g_ubus
local g_limit={}

local QOS_ACK,QOS_SYN,QOS_FIN,QOS_RST,QOS_ICMP,QOS_Small

local g_QOS_Status=0  -- 0 no qdisc, 1 only prio, 2 full stack
local g_close_layer1 =0  -- prio qdisc is on as default

-------constant string-----------
local const_ipt_mangle="iptables -t mangle "
local const_ipt_clear="iptables -t mangle -F "
local const_ipt_delete="iptables -t mangle -X "
local const_tc_qdisc="tc qdisc"
local const_tc_class="tc class"
local const_tc_filter="tc filter"
local const_tc_dump=' tc -d class show dev %s |grep "level 5" '

local IPT_CHAIN_ROOT="miqos_fw"
local CHILD_IPT_CHAIN_ROOT="miqos_cg"
local UNIT="kbit"

local g_htb_parent_classid, g_xq_tbf_parent_classid, g_guest_tbf_parent_classid
local g_htb_major_id={UP='',DOWN=''}
local g_htb_parent={UP='',DOWN=''}

-- 分类filter的优先级定义表
local g_filter_prio_tab={
    ['PRIO_SUBNET_HIGH']= '1',      -- subnet高优先级
    ['PRIO_HOST_ONLY_HIGH']= '2',   -- host高优先级设置
    ['PRIO_SUBNET_LOW']='3',        -- subnet低优先级
    ['PRIO_U32_HIGH']= '4',         -- U32高优先级
    ['PRIO_U32_LOW']= '5',          -- U32低优先级
    ['PRIO_CUSTOM']= '6',           -- 自定义优先级设置，如TPG游戏
    ['PRIO_HOST_AND_TYPE']= '8',    -- host+流类型优先级匹配
    ['PRIO_HOST_ONLY_LOW']= '9',    -- host低优先级，host对应的默认最低优先级
    ['PRIO_LOWEST']= '10',          -- 其他
}

local const_NO_LIMIT_HIGH='PRIO_HTB_NO_LIMIT_HIGH'
local const_BAND_LIMIT_HIGH='PRIO_HTB_BAND_LIMIT_HIGH'
local const_QDISC_PRIO_DEFAULT_HIGH='PRIO_PRIO_DEFAULT_HIGH'

local g_filter_flowid_tab={
    [const_NO_LIMIT_HIGH] = '0',           -- htb结构下无限制到根节点
    [const_BAND_LIMIT_HIGH] = '2003',      -- htb结构下只限制band到class 2003类
    [const_QDISC_PRIO_DEFAULT_HIGH] = '2',           -- prio结构下所有高优先级配置都到clas 2 类
}

-- 高优先级的host列表
local g_high_prio_host={
    -- [ip]=flow-id
};
local g_high_prio_host_changed=false

-- 顶层的htb结构layer1
local g_htb_layer1={
    ['prio']={ -- high prio packet
        id=0x2000,
        prio=1,
        quan=4,
        fwmark='', --所有数据包从叶子节点流走
        [UP]={rate=0.30, ceil=1.0, },
        [DOWN]={rate=0.30, ceil=1.0,  },
    },
    ['host']={ -- host packet
        id=0x3000,
        prio=2,
        quan=3,
        fwmark='',
        [UP]={rate=0.50, ceil=1.05,  },
        [DOWN]={rate=0.50, ceil=1.05,  },
    },
    ['guest']={ -- guest packet
        id=0x4000,
        prio=6,
        quan=2,
        fwmark='0x00040000/0x000f0000',
        [UP]={rate=0.20, ceil=g_guest_default_band_factor,  },
        [DOWN]={rate=0.20, ceil=g_guest_default_band_factor,  },
    },
    ['xq']={ -- xq packet and nomark
        id=0x5000,
        prio=7,
        quan=1,
        fwmark='0x00050000/0x000f0000',
        [UP]={rate=0.05, ceil=g_xq_default_band_factor,  },
        [DOWN]={rate=0.05, ceil=g_xq_default_band_factor,  },
    },
}

-- 自定义优先级的队列配置, one of layer2
local g_htb_layer2_prio={
    [1]={
        id=0x1,
        prio=1,
        quan=4,
        fwmark='0x00120000/0x00ff0000',
        [UP]={rate=0.4,ceil=1.0, },
        [DOWN]={rate=0.4,ceil=1.0, },
    },
    [2]={
        id=0x2,
        prio=1,
        quan=3,
        fwmark='0x00220000/0x00ff0000',
        [UP]={rate=0.3,ceil=1.0, },
        [DOWN]={rate=0.3,ceil=1.0, },
    },
    [3]={
        id=0x3,
        prio=1,
        quan=2,
        prio_packets=1,
        fwmark='0x00320000/0x00ff0000',
        [UP]={rate=0.3,ceil=1.0, },
        [DOWN]={rate=0.3,ceil=1.0, },
    },
}

-- host队列的优先级和rate/ceil配置
local g_htb_layer2_host={
    [1]={
        prio=2,
        rate=0.2,
        ceil=0.95,
    },
    [2]={
        prio=3,
        rate=0.2,
        ceil=0.95,
    },
    [3]={
        prio=4,
        rate=0.4,
        ceil=0.95,
    },
    [4]={
        prio=5,
        rate=0.1,
        ceil=0.95,
    },
    [5]={
        prio=6,
        rate=0.2,
        ceil=0.95,
    },
}

local g_host_rate={[UP] = 0, [DOWN] = 0 }
local g_host_ceil={[UP] = 0, [DOWN] = 0 }

local cfg_dir='/etc/config/'
local tmp_cfg_dir='/tmp/etc/config/'
local cfg_file=cfg_dir .. 'miqos'
local tmp_cfg_file=tmp_cfg_dir .. 'miqos'
local g_cursor

-- 读取cfg到tmp的meory文件夹中
function cfg2tmp()
    local r1,r2,r3 = fs.mkdirr(tmp_cfg_dir)
    if not r1 then
        logger(3, 'fatal error: mkdir failed, code:' .. r2 .. ',msg:'..r3)
        return nil
    end

    r1,r2,r3 = fs.copy(cfg_file,tmp_cfg_file)
    if not r1 then
        logger(3,'fatal error: copy cfg file 2 /tmp memory failed. code:' .. r2 .. ',msg:'..r3)
        return nil
    end
    return true
end

-- 十进制转十六进制
function dec2hexstr(d)
    return string.format("%x",d)
end

-- 拷贝最新配置到memory中
function tmp2cfg()
    if not fs.copy(tmp_cfg_file,cfg_file) then
        logger(3,'fatal error: copy /tmp cfg file 2 /etc/config/ failed. exit.')
        return nil
    end
    return true
end

function copytab(st)
    local tab={}
    for k,v in pairs(st or {}) do
        if type(v) ~= 'table' then tab[k]=v
        else tab[k]=copytab(v) end
    end
    return tab
end

function get_conf_std(conf,type,opt,default)
    local x=uci.cursor()
    local s,e = pcall(function() return x:get(conf,type,opt) end)
    return e or default
end

function get_tbls(conf,type)
    local tbls={}
    local s,e = pcall(function() g_cursor:foreach(conf, type, function(s) tbls[s['name']]=s end) end)
    return tbls or {}
end

-- execute command without anyoutput
function exec_cmd(tblist, ignore_error)
    local status = 0
    for _,v in pairs(tblist) do
        local cmd = v

        if g_debug then
            logger(3, '++' .. cmd)
            cmd = cmd .. ' >/dev/null 2>>' .. g_outlog
        else
            cmd = cmd .. " &>/dev/null "
        end

        if os.execute(cmd) ~= 0 and ignore_error ~= 1 then
            if g_debug then
                os.execute('echo "^^^ '.. cmd .. ' ^^^ " >>' .. g_outlog)
            end
            logger(3, '[ERROR]:  ' .. cmd .. ' failed!')
            dump_qdisc()
            return false
        end
    end

    return true
end

function newset()
    local reverse = {}
    local set = {}
    return setmetatable(set, {__index = {
        insert = function(set, value)
            if not reverse[value] then
                table.insert(set, value)
                reverse[value] = table.getn(set)
            end
        end,
        remove = function(set, value)
            local index = reverse[value]
            if index then
                reverse[value] = nil
                local top = table.remove(set)
                if top ~= value then
                    reverse[top] = index
                    set[index] = top
                end
            end
        end
    }})
end

--split string with chars '$p'
string.split = function(s, p)
    local rt= {}
    string.gsub(s, '[^'..p..']+', function(w) table.insert(rt, w) end )
    return rt
end

-- 出错后dump规则用于除错
function dump_qdisc()
    local tblist={}
    local expr=''
    table.insert(tblist,'tc -d qdisc show | sort ')

    local devs={'eth0.2','ifb0'}
    for _,dev in pairs(devs) do
        table.insert(tblist, 'tc -d class show dev ' .. dev .. ' | sort ')
    end
    for _,dev in pairs(devs) do
        table.insert(tblist, 'tc -d filter show dev ' .. dev)
    end
    logger(3, '--------------miqos error dump START--------------------')
    local pp,data
    for _,cmd in pairs(tblist) do
        pp=io.popen(cmd)
        if pp then
            for d in pp:lines() do
                logger(3, d)
            end
        end
    end
    pp:close()
    logger(3, '--------------miqos error dump END--------------------')
end

-- 清除iptables mark规则
function host_ipt_cleanup()
    -- only clear QOSC link rules
    local expr=string.format(" %s -F %s ", const_ipt_mangle, CHILD_IPT_CHAIN_ROOT)
    local tblist={}
    table.insert(tblist,expr)
    if not exec_cmd(tblist,1) then
        logger(3, 'clean host ipt rules failed!')
    end

end

-- layer1 清除qdisc所有规则
function cleanup_layer1_root()
    local tblist={}
    local devs={'ifb0','eth0.2'}
    local expr
    for seq,dev in pairs(devs) do
        expr=string.format(" %s del dev %s root ", const_tc_qdisc, dev)
        table.insert(tblist,expr)
    end

    if not exec_cmd(tblist,1) then logger(3, 'clean all qdisc rules failed!') end

    g_htb_layer1_ready = false
    g_prio_layer1_ready=false
    g_htb_layer2_host_ready=false
end

-- layer2 清除目前host下的所有节点
function cleanup_htb_layer2_host()
    local g_flow_type=update_flow_type()
    if g_crt_ipmac_map then
        for seq, flow_item in pairs(g_flow_type) do
            delete_all_hosts_qos(flow_item['dev'], flow_item['direction'], flow_item['root'])
        end

        -- 清除当前已记录的host列表，表示host规则已经清除
        g_crt_ipmac_map=nil
    end

    host_ipt_cleanup()

    g_htb_layer2_host_ready = false
end

-- 清除所有2层规则
function cleanup_system()
    cleanup_layer1_root()
    host_ipt_cleanup()
    g_crt_ipmac_map=nil
    logger(3,'======= Cleanup QoS rules.=====')
end

-- 清除所有2层规则，并退出服务
function system_exit()
    logger(3,'======== Process Exit. =====')
    cleanup_system()
    os.exit()
end

function system_init()
    if g_debug then
        os.execute("echo miqos starting..... >>" .. g_outlog)
    end
    -- 将配置文件copy到tmp内存中,并初始化cursor
    if not cfg2tmp() then
        return false
    end

    g_cursor = uci.cursor()
    if not g_cursor or not g_cursor:set_confdir(tmp_cfg_dir) then
        logger(3,'set tmp config dir failed. exit.')
    end

    g_ubus = ubus.connect()
    if not g_ubus then
        logger(3, 'failed to connect to ubusd!')
        return false
    end

    if not read_qos_config() then
        logger(3, 'read config failed, exit!')
        return false
    end

    -- SIGTERM to clear and exit
    px.signal(px.SIGTERM,
        function ()
            logger(3,'signal TERM to stop miqos.')
            system_exit()
        end)

    px.signal(px.SIGINT,
        function ()
            logger(3,'signal INT to stop miqos.')
            system_exit()
        end)

    -- 启动读取系统配置
    read_sys_conf()

    return true
end

function read_network_conf()
    g_lan_if=g_ifb
    g_wan_if='eth0.2'

    local tmp = get_conf_std('network','wan','proto')
    if tmp == 'dhcp' or tmp == 'static' then
        g_wan_type = 'ip'
    elseif tmp == 'pppoe' then
        g_wan_type = 'pppoe'
    else
        logger(1, 'cannot determine wan interface! exit')
        return false
    end
end

-- 系统相关的配置文件
function read_sys_conf()
    read_network_conf()

    local config_tbl = get_tbls('miqos','system')
    cfg_qos_check_interval=config_tbl['param']['interval'] or '5'
    cfg_default_group=config_tbl['param']['default_group'] or '00'   -- 00 is default group
    cfg_default_mode=config_tbl['param']['default_mode'] or 'general'

    -- if check interval less than 3sec, will reset it to 3sec
    if not cfg_qos_check_interval or tonumber(cfg_qos_check_interval) < 5 then
        cfg_qos_check_interval = 5
    else
        cfg_qos_check_interval = tonumber(cfg_qos_check_interval)
    end

end

--读取qos相关的配置
function read_qos_config()

    local setting_tbl = get_tbls('miqos','miqos')

    cfg_bandwidth[UP]=setting_tbl['settings']['upload'] or '0'
    cfg_bandwidth[DOWN]=setting_tbl['settings']['download'] or '0'
    cfg_qos_enabled=setting_tbl['settings']['enabled'] or '0'

    QOS_ACK=setting_tbl['settings']['qos_ack'] or '0'
    QOS_SYN=setting_tbl['settings']['qos_syn'] or '0'
    QOS_FIN=setting_tbl['settings']['qos_fin'] or '0'
    QOS_RST=setting_tbl['settings']['qos_rst'] or '0'
    QOS_ICMP=setting_tbl['settings']['qos_icmp'] or '0'
    QOS_Small=setting_tbl['settings']['qos_small'] or '0'
    QOS_TYPE=setting_tbl['settings']['qos_auto'] or 'auto'

    g_guest_max_band[UP],g_guest_max_band[DOWN] = get_guest_percent()
    group_def=get_tbls('miqos','group')

    -- 一组只含有一个MAC,同一个MAC可以对应多个IP
    -- for k,v in pairs(group_def) do
    --     for m,n in pairs(v['mac']) do
    --         group_def[k]['mac'][m] = string.upper(n)
    --     end
    -- end

    -- QOS_TYPE: auto 最小保证和最大带宽设置均无效， min 最小保证设置有效， max 最大带宽设置有效, both 最大最小均可调整
    -- 自动模式设置组为group 00
    if QOS_TYPE == 'auto' then
        for k,v in pairs(group_def) do
            if v['name'] ~= cfg_default_group then
                group_def[k] = nil
            end
        end
    elseif QOS_TYPE == 'min' then
        for k,v in pairs(group_def) do
            if v['name'] ~= cfg_default_group then
                group_def[k]['max_grp_uplink'] = 0
                group_def[k]['max_grp_downlink'] = 0
            end
        end
    elseif QOS_TYPE == 'max' then
        for k,v in pairs(group_def) do
            if v['name'] ~= cfg_default_group then
                group_def[k]['min_grp_uplink'] = 0
                group_def[k]['min_grp_downlink'] = 0
            end
        end
    elseif QOS_TYPE == 'both' then
        -- keep config for both changes.
    else
        -- if not, set it as auto mode, and sync to config
        QOS_TYPE='auto'
        update_qos_auto_mode(QOS_TYPE, true)
        for k,v in pairs(group_def) do
            if v['name'] ~= cfg_default_group then
                group_def[k] = nil
            end
        end
    end

    -- limit配置的检查
    for k, v in pairs(group_def) do
        if tonumber(v['min_grp_uplink']) < 0 or tonumber(v['min_grp_uplink']) > 1 then
            group_def[k]['min_grp_uplink'] = 0
        end
        if tonumber(v['min_grp_downlink']) < 0 or tonumber(v['min_grp_downlink']) > 1 then
            group_def[k]['min_grp_downlink'] = 0
        end
        if tonumber(v['max_grp_uplink']) < 8 then         -- 8kbps = 1kb/s
            group_def[k]['max_grp_uplink'] = 0
        end
        if tonumber(v['max_grp_downlink']) < 8 then
            group_def[k]['max_grp_downlink'] = 0
        end
    end


    mode_def=get_tbls('miqos','mode')
    class_def=get_tbls('miqos','class')

    return true
end

-- 计算最终使用的bandwidth
function determine_band()

    -- 读取cfg的band
    local tobe_bandwidth={UP=tonumber(cfg_bandwidth[UP]),DOWN=tonumber(cfg_bandwidth[DOWN])}

    if tobe_bandwidth[UP] > 0 or tobe_bandwidth[DOWN] > 0 then
        if g_extend_flag then
            -- 读取rec的band
            rec_bandwidth=update_rec_band()

            if tobe_bandwidth[UP] < rec_bandwidth[UP] then
                logger(3,"---- UP: cfg bandwidth:" .. tobe_bandwidth[UP] .. ' < rec_bandwidth:'..rec_bandwidth[UP]..'.')
                tobe_bandwidth[UP] = rec_bandwidth[UP]
            end
            if tobe_bandwidth[DOWN] < rec_bandwidth[DOWN] then
                logger(3,"---- DOWN: cfg bandwidth:" .. tobe_bandwidth[DOWN] .. ' < rec_bandwidth:'..rec_bandwidth[DOWN]..'.')
                tobe_bandwidth[DOWN] = rec_bandwidth[DOWN]
            end
        end
    else
        tobe_bandwidth[UP] = 0
        tobe_bandwidth[DOWN] = 0
    end

    return tobe_bandwidth
end

-- 检测Host列表变化，仅仅会触发layer_host规则重刷
function check_host_changed()
    local ret=false

    if cfg_changed == 'HOST' then
        logger(3, '[HOST CHANGED] changed by command.')
        cfg_changed='NONE'
        ret = true
    end

    local update = update_host_list()
    if update == 1 or cfg_changed == 'HOST' then
        logger(3, '[HOST CHANGED]: changed by service detected.')
        ret=true
    end

    return ret
end

--检测配置和band变化,会触发layer1以下所有规则重刷
function check_status_changed()
    local ret=false
    local act_bandwidth = determine_band()

    if cfg_qos_enabled ~= cur_qos_enabled then
        logger(3,'[QOS FLAG CHANGED] flag:' .. cur_qos_enabled .. '->' .. cfg_qos_enabled)
        ret = true
    end

    if g_wan_type ~= g_cur_wan_type then
        logger(3,'[WAN TYPE CHANGED] flag:' .. g_cur_wan_type .. '->' .. g_wan_if)
        g_cur_wan_type = g_wan_type
        ret = true
    end

    if cfg_changed == 'ALL' then
        logger(3, '[CONFIG CHANGED] changed by command.')
        cfg_changed='NONE'
        ret = true
    end

    if act_bandwidth[UP] ~= tc_bandwidth[UP] or act_bandwidth[DOWN] ~= tc_bandwidth[DOWN] then
        logger(3, '[BAND CHANGED] Uplink:' .. tc_bandwidth[UP] .. '->' .. act_bandwidth[UP] ..', Downlink:' .. tc_bandwidth[DOWN] .. '->' .. act_bandwidth[DOWN])
        tc_bandwidth[UP] = act_bandwidth[UP]
        tc_bandwidth[DOWN] = act_bandwidth[DOWN]

        cur_bandwidth[UP] = math.ceil(tc_bandwidth[UP] * band_extend_factor)
        cur_bandwidth[DOWN] = math.ceil(tc_bandwidth[DOWN] * band_extend_factor)
        ret = true
    end

    if g_guest_max_band[UP] ~= g_cur_guest_max_band[UP] or g_guest_max_band[DOWN] ~= g_cur_guest_max_band[DOWN] then
        logger(3, '[GUEST CHANGED] Uplink:' .. g_cur_guest_max_band[UP] .. '->' .. g_guest_max_band[UP] .. ', Downlink:' .. g_cur_guest_max_band[DOWN] .. '->' .. g_guest_max_band[DOWN])
        g_cur_guest_max_band[UP] = g_guest_max_band[UP]
        g_cur_guest_max_band[DOWN] = g_guest_max_band[DOWN]
        ret = true
    end

    return ret
end

-- 更新host列表简版,仅供qos off时调用，用于清理high-prio-host-list
function update_host_list_compact()
    g_nxt_ipmac_map = {}
    g_num_of_clients = 0
    -- 获取active的mac和ip信息
    local ret=g_ubus:call("trafficd", "hw", {})

    -- step1.更新nxt ipmac表
    for mac,v in pairs(ret or {}) do
        local mac = v['hw']
        local wifi = '0'
        if v['ifname'] == "wl0" or v['ifname'] == "wl1" then
            wifi = '1'  -- host wifi
        elseif string.find(v['ifname'],"wl",1) then
            wifi = '2'   -- guest wifi, will be skipped later
        end
        -- 遍历此mac下所有ip地址
        for k,ips in pairs(v['ip_list'] or {}) do
            -- 检查ip地址的在线状态by ageingtime
            local valid_ip = false
            if wifi == '1' then
                if v['assoc'] == 1 and ips['ageing_timer'] < g_idle_timeout then
                    valid_ip = true
                end
            elseif wifi == '0' then
                if ips['ageing_timer'] < g_idle_timeout then
                    valid_ip = true
                end
            end

            local ip = ips['ip']
            local nid=string.split(ip,'.')[4]
            -- 局点有效,首先置此ip为NEW
            if valid_ip and nid then
                -- 维护队列,以ip为key
                g_nxt_ipmac_map[ip]={
                    mac=mac,
                    st='S_NEW',
                    id=nid,
                    idle=ips['ageing_timer'],
                }
                g_num_of_clients = g_num_of_clients + 1

                if not g_alive_hosts[mac] then
                    g_alive_hosts[mac] = {}
                end

                -- mac + ip 表示一个有效的host
                g_alive_hosts[mac][ip] = 1
                -- logger(3,"mac: " .. mac .. ',ip: ' .. ip)
            end
        end
    end

    -- 判断high-prio-host是否在线有效
    --[[ 只利用dhcp过期的
    for k, v in pairs(g_high_prio_host) do
        if not g_nxt_ipmac_map[k] then  -- 无效host，则从high-prio-table中清除
            tc_update_host_prio_and_limit('del', k, nil)  -- 删除过期的host优先级记录
        end
    end
    --]]
end

-- update actived host list
-- 如果新的ip和mac跟已存的信息不一致的情况,则清除原来的ip和mac对应的规则,然后新建规则
function update_host_list()

    local host_changed=0

    g_num_of_clients = 0
    g_alive_hosts={}

    -- step1
    update_host_list_compact()

    -- step2.根据crt ipmac表 和 nxt ipmac表,设置nxt表中需更新的IP状态记录
    for iip,imac in pairs(g_nxt_ipmac_map or {}) do
        if g_crt_ipmac_map and g_crt_ipmac_map[iip] then
            -- 此IP规则需要更新
            g_nxt_ipmac_map[iip]['st']='S_UPD'

            -- IP未变,但是对应的MAC变化了,则需要刷新
            if g_crt_ipmac_map[iip]['mac'] ~= imac['mac'] then
                host_changed = 1
                logger(3,'ip-mac mapping changed ' .. iip .. '<>' .. g_crt_ipmac_map[iip]['mac'] .. '/'.. imac['mac'] ..' triggered fresh')
            end
        else
            -- 新加入的IP,需要刷新
            host_changed = 1
            logger(3,'new ip ' .. iip ..' triggered fresh')
        end
    end

    -- step3.根据crt ipmac表 和 nxt ipmac表,设置crt表中需要删除的IP状态记录
    for iip,imac in pairs(g_crt_ipmac_map or {}) do
        if g_nxt_ipmac_map[iip] then
            g_crt_ipmac_map[iip]['st']='S_NONE'
        else
            -- [分类2].有客户端过期,需要刷新客户端规则
            g_crt_ipmac_map[iip]['st']='S_DEL'
            host_changed = 1
            logger(3,'remove expired ip ' .. iip ..' triggered fresh')
        end
    end

    return host_changed
end


-- 带宽分配功能
function arrange_bandwidth()

    local host_counter_with_cfg = 0
    local host_counter_without_cfg = 0

    -- calculate total quantums of rate
    local total_quantum_up=0.0
    local total_quantum_down=0.0
    for k,v in pairs(g_alive_hosts) do
        local host_counter_this_mac = 0
        for _,_ in pairs(v) do
            host_counter_this_mac = host_counter_this_mac + 1
        end
        if g_debug then
            logger(3,'alive host number of mac [' .. k .. '] : ' .. host_counter_this_mac)
        end

        local group_id
        if not group_def[k] then
            group_id = cfg_default_group   -- 无配置,则设定为默认分组
            host_counter_without_cfg = host_counter_without_cfg + host_counter_this_mac
        else
            group_id = k
            group_def[k]['count'] = host_counter_this_mac
            host_counter_with_cfg = host_counter_with_cfg + host_counter_this_mac
        end

        local tmp_num = tonumber(group_def[group_id]['min_grp_uplink'])
        if tmp_num <=0 or tmp_num > 1 then
            group_def[group_id]['min_grp_uplink'] = g_default_min_updown_factor
            tmp_num = tonumber(g_default_min_updown_factor)
        end
        total_quantum_up = total_quantum_up + tmp_num * host_counter_this_mac

        tmp_num = tonumber(group_def[group_id]['min_grp_downlink'])
        if  tmp_num <= 0 or tmp_num > 1 then
            group_def[group_id]['min_grp_downlink'] = g_default_min_updown_factor
            tmp_num = tonumber(g_default_min_updown_factor)
        end
        total_quantum_down = total_quantum_down + tmp_num * host_counter_this_mac

    end

    group_def[cfg_default_group]['count'] = host_counter_without_cfg

    for k,v in pairs(group_def) do
        if v['count'] and tonumber(v['count']) > 0 then

            --minimum assured rate
            if not v['min_grp_uplink'] or v['min_grp_uplink'] == '0' then
                v['min_grp_uplink'] = g_default_min_updown_factor
            end
            group_def[k]['each_up_rate'] = v['min_grp_uplink'] / total_quantum_up

            if not v['min_grp_downlink'] or v['min_grp_downlink'] == '0' then
                v['min_grp_downlink'] = g_default_min_updown_factor
            end
            group_def[k]['each_down_rate'] =  v['min_grp_downlink'] / total_quantum_down

            --maximum ceil rate
            if not v['max_grp_uplink'] or  v['max_grp_uplink'] == '0' then
                v['max_grp_uplink'] = 0
            end
            group_def[k]['each_up_ceil'] = v['max_grp_uplink'] / const_total_max_grp_ratio

            if not v['max_grp_downlink'] or  v['max_grp_downlink'] == '0' then
                v['max_grp_downlink'] = 0
            end
            group_def[k]['each_down_ceil'] =  v['max_grp_downlink'] / const_total_max_grp_ratio

            logger(3, '@mac:' .. k .. ',count:' .. v['count'] .. ',min_up:' .. v['each_up_rate'] .. ',min_down:' .. v['each_down_rate'] .. ',max_up:' .. v['each_up_ceil'] .. ',max_down:' .. v['each_down_ceil'])
        end
    end

    return host_counter_with_cfg + host_counter_without_cfg
end

-- 根据配置的ports/tos等对skb打mark
function ipt_mark_hosts_nf()

    local expr=''
    local tblist={}
    -- by design, only support default mode
    local host_mode= mode_def[cfg_default_mode]
    local tcp_ports, udp_ports, tos

    for m=1, #(host_mode['subclass']) do
        local cls = class_def[host_mode['subclass'][m]]
        if cls then
            local as_default = 1
            tcp_ports=cls['tcp_ports']
            udp_ports=cls['udp_ports']
            tos=cls['tos']
            if tcp_ports and tcp_ports ~= '' then
                expr = string.format(" %s -A %s -m mark --mark 0/0x00f00000 -p tcp -m multiport --ports %s -j MARK --set-mark-return 0x%s00000/0x00f00000 ", const_ipt_mangle, CHILD_IPT_CHAIN_ROOT, tcp_ports, m)
                table.insert(tblist,expr)
                as_default = 0
            end

            if udp_ports and udp_ports ~= '' then
                expr = string.format(" %s -A %s -m mark --mark 0/0x00f00000 -p udp -m multiport --ports %s -j MARK --set-mark-return 0x%s00000/0x00f00000 ", const_ipt_mangle, CHILD_IPT_CHAIN_ROOT, udp_ports, m)
                table.insert(tblist,expr)
                as_default = 0
            end

            if tos and tos ~= '' then
                expr = string.format(" %s -A %s -m mark --mark 0/0x00f00000 -m tos --tos %s -j MARK --set-mark-return 0x%s00000/0x00f00000 ", const_ipt_mangle, CHILD_IPT_CHAIN_ROOT, tos, m)
                table.insert(tblist,expr)
                as_default = 0
            end

            if as_default ~= 0 then
                expr = string.format(" %s -A %s -m mark --mark 0/0x00f00000 -j MARK --set-mark-return 0x%s00000/0x00f00000 ", const_ipt_mangle, CHILD_IPT_CHAIN_ROOT, m)
                table.insert(tblist,expr)
            end
        end
    end

    if not exec_cmd(tblist,nil) then
        logger(3,'ERROR: ipt mark hosts nf failed.')
        return false
    end
    return true
end

-- 更新对应host的优先级或带宽限制
function tc_update_host_prio_and_limit(act, ip, flowid)
    if act == 'add' then
        g_high_prio_host[ip]=flowid
        if g_debug then
            logger(3, '+++ add-host-prio, ' .. ip .. '：' .. flowid)
        end
    elseif act == 'del' then
        g_high_prio_host[ip]=nil
        if g_debug then
            logger(3, '--- del-host-prio, ' .. ip )
        end
    else
        logger(3, 'not supported update host prio and limit action: ' .. act);
        return false
    end
    g_high_prio_host_changed = true
    return true
end

-- 对于此类优先级更新，先删除此类优先级规则，然后再添加filter，更简洁 :-)
function tc_update_host_prio_and_limit_filter(dev, parent, is_htb)
    local tblist={}
    local handle_str,flowid_str='',''

    if g_debug then
        logger(3, '--------update-host-highest-prio-------------')
    end

    -- delete all filter 1stly
    local expr=string.format("%s del dev %s parent %s: prio %s ", const_tc_filter, dev, parent, g_filter_prio_tab['PRIO_HOST_ONLY_HIGH'])
    table.insert(tblist,expr)

    -- 忽略执行错误的删除filter记录
    exec_cmd(tblist,1)
    tblist={}

    -- add all necessary filter
    for k, v in pairs(g_high_prio_host) do
        local nid=string.split(k,'.')[4]
        local handle_str = '0x' .. dec2hexstr(nid) .. '000000/0xff000000'
        if is_htb then
            flowid_str = g_filter_flowid_tab[v]        -- htb队列，根据配置进行归集（->:0, ->:2003）
        else
            flowid_str = g_filter_flowid_tab[const_QDISC_PRIO_DEFAULT_HIGH]    -- prio队列，自动归集到:2优先级队列
        end
        expr = string.format(" %s replace dev %s parent %s: prio %s handle %s fw classid %s:%s ", const_tc_filter, dev, parent, g_filter_prio_tab['PRIO_HOST_ONLY_HIGH'], handle_str, parent, flowid_str)
        table.insert(tblist, expr)
    end

    if not exec_cmd(tblist,nil) then
        logger(3,'ERROR: update host_prio_and_limit_filter failed.')
        system_exit()
        -- exit system when fatal error
    end
    return true
end

function tc_add_special_filter(tblist, act, dev, flow_id, dir, high_class_id)

    local proto_id,offset_v,priority = 'all',0, g_filter_prio_tab['PRIO_U32_LOW']

    if g_cur_wan_type == 'pppoe' then
        offset_v = 8
    end

    __tc_add_special_filter(tblist, act, dev, flow_id, dir, high_class_id, priority, proto_id, offset_v)

    if g_cur_wan_type == 'pppoe' and QOS_VER == 'CTF' and dir == DOWN then
        proto_id = '0x0800'
        offset_v = 0
        priority = g_filter_prio_tab['PRIO_U32_HIGH']
        __tc_add_special_filter(tblist, act, dev, flow_id, dir, high_class_id, priority, proto_id, offset_v)
    end

end

-----------------------------------------------------------------------------
-- 为下面的包增加高优先级队列
-- 1. ACK/SYN/FIN 1stly
-- 2. length < 64 bytes (small packet)
-----------------------------------------------------------------------------
function __tc_add_special_filter(tblist, act, dev, flow_id, dir, high_class_id, priority, proto_id, offset_v)
    local expr=''
    local highest_class_handle = high_class_id

    if QOS_ACK == '1' then
        expr=string.format(" %s %s dev %s parent %s: prio %s protocol %s u32 match u8 0x05 0x0f at %d match u16 0x0000 0xffc0 at %d match u8 0x10 0xff at %d flowid %s:%s ", const_tc_filter, act, dev, flow_id, priority, proto_id, 0+offset_v, 2+offset_v, 33+offset_v, flow_id, highest_class_handle)
        table.insert(tblist,expr)
    end

    if QOS_SYN == '1' then
        expr=string.format(" %s %s dev %s parent %s: prio %s protocol %s u32 match u8 0x05 0x0f at %d match u16 0x0000 0xffc0 at %d match u8 0x02 0x02 at %d flowid %s:%s ", const_tc_filter, act, dev, flow_id, priority, proto_id, 0+offset_v, 2+offset_v, 33+offset_v, flow_id, highest_class_handle)
        table.insert(tblist,expr)
    end

    if QOS_FIN == '1' then
        expr=string.format(" %s %s dev %s parent %s: prio %s protocol %s u32 match u8 0x05 0x0f at %d match u16 0x0000 0xffc0 at %d match u8 0x01 0x01 at %d flowid %s:%s ", const_tc_filter, act, dev, flow_id, priority, proto_id, 0+offset_v, 2+offset_v, 33+offset_v, flow_id, highest_class_handle)
        table.insert(tblist,expr)
    end

    if QOS_RST == '1' then
        expr=string.format(" %s %s dev %s parent %s: prio %s protocol %s u32 match u8 0x05 0x0f at %d match u16 0x0000 0xffc0 at %d match u8 0x04 0x04 at %d flowid %s:%s ", const_tc_filter, act, dev, flow_id, priority, proto_id, 0+offset_v, 2+offset_v, 33+offset_v, flow_id, highest_class_handle)
        table.insert(tblist,expr)
    end

    if QOS_ICMP == '1' then
        expr=string.format(" %s %s dev %s parent %s: prio %s protocol %s u32 match u8 0x01 0xff at %d flowid %s:%s ",  const_tc_filter, act, dev, flow_id, priority, proto_id, 9+offset_v, flow_id,highest_class_handle)
        table.insert(tblist,expr)
    end

    -- small packakages if length <  64 bytes for tcp
    if QOS_Small == '1' then
        local mask = '0xffc0'

        expr=string.format(" %s %s dev %s parent %s: prio %s protocol %s u32 match u16 0x0000 %s at %d flowid %s:%s ", const_tc_filter, act, dev, flow_id, priority, proto_id, mask, 2+offset_v, flow_id,  highest_class_handle)
        table.insert(tblist,expr)
    end

    return true
end

-- 构建layer1的prio结构, it's fixed without changes.
function tc_fixed_layer1_prio_root_qdisc_class_filters(dev, flow_id, act, ratelimit, direction)
    local tblist={}
    local expr=''
    local mg_parent,customer_parent,host_parent, guest_tbf_parent,xq_tbf_parent = '1','2','3','4','5'
    if act == 'add' then
        expr=string.format(" %s %s dev %s root handle %s: prio bands 5 priomap 2 3 3 3 2 3 1 1 2 2 2 2 2 2 2 2 ", const_tc_qdisc, act, dev, flow_id)
        table.insert(tblist,expr)

        -- filter for messagent + customized prio
        expr=string.format(" %s %s dev %s parent %s: prio %s handle %s fw classid %s:%s ",
                const_tc_filter, act, dev, flow_id, g_filter_prio_tab['PRIO_SUBNET_LOW'], '0x00010000/0x000f0000', flow_id, mg_parent)
        table.insert(tblist,expr)

        expr=string.format(" %s %s dev %s parent %s: prio %s handle %s fw classid %s:%s ",
                const_tc_filter, act, dev, flow_id, g_filter_prio_tab['PRIO_SUBNET_LOW'], '0x00020000/0x000f0000', flow_id, customer_parent)
        table.insert(tblist,expr)

        -- filter for host
        expr=string.format(" %s %s dev %s parent %s: prio %s handle %s fw classid %s:%s ",
                const_tc_filter, act, dev, flow_id, g_filter_prio_tab['PRIO_SUBNET_LOW'], '0x00030000/0x000f0000', flow_id, host_parent)
        table.insert(tblist,expr)

        -- filter for guest
        expr=string.format(" %s %s dev %s parent %s: prio %s handle %s fw classid %s:%s ",
                const_tc_filter, act, dev, flow_id, g_filter_prio_tab['PRIO_SUBNET_LOW'], '0x00040000/0x000f0000', flow_id, guest_tbf_parent)
        table.insert(tblist,expr)


        -- filter for xq
        expr=string.format(" %s %s dev %s parent %s: prio %s handle %s fw classid %s:%s ",
                const_tc_filter, act, dev, flow_id, g_filter_prio_tab['PRIO_SUBNET_LOW'], '0x00050000/0x000f0000', flow_id, xq_tbf_parent)
        table.insert(tblist,expr)
    end

    act='replace'

    -- rate limit for guest and xq
    local guest_ratelimit
    -- 如果guest band <=1, 则认为取的是百分比，否则为绝对值
    if g_cur_guest_max_band[direction] <= 1 then
        guest_ratelimit=math.ceil(ratelimit*g_cur_guest_max_band[direction])
    else
        guest_ratelimit=math.ceil(g_cur_guest_max_band[direction])
    end

    local buffer=math.ceil(guest_ratelimit*1024/g_CONFIG_HZ)
    if buffer < 2000 then buffer = 2000 end
    expr=string.format(" %s %s dev %s parent %s:%s handle %s0: tbf rate %s%s buffer %s latency 10ms",const_tc_qdisc, act, dev, flow_id, guest_tbf_parent, flow_id, guest_ratelimit, UNIT, buffer)
    table.insert(tblist,expr)

    local xq_ratelimit=math.ceil(ratelimit*g_xq_default_band_factor)
    buffer=math.ceil(xq_ratelimit*1024/g_CONFIG_HZ)
    if buffer < 2000 then buffer = 2000 end
    expr=string.format(" %s %s dev %s parent %s:%s handle %s1: tbf rate %s%s buffer %s latency 10ms",const_tc_qdisc, act, dev, flow_id, xq_tbf_parent, flow_id, xq_ratelimit, UNIT, buffer)
    table.insert(tblist,expr)

    if not exec_cmd(tblist,nil) then
        logger(3,'ERROR: add prio root TC-layer1 qdisc rules failed.')
        system_exit()
        -- exit system when fatal error
    end
    return true
end

-- 构建layer1的htb结构
function tc_fixed_layer1_htb_root_qdisc_class_filters(dev, flow_id, act, ratelimit, direction)
    local tblist={}
    local expr=''
    local parent='1000'
    local cbuffer = math.ceil(tonumber(ratelimit)*1024/8.0/g_CONFIG_HZ*g_htb_buffer_factor)
    local buffer = math.ceil(cbuffer)

    if act ~= 'add' and act ~= 'change' then
        logger(3,"not supported act: ".. act .. ' in layer1 htb root.')
        return false
    end

    if act == 'add' then   -- only add for qdisc
        local tmp_xq_id=dec2hexstr(g_htb_layer1['guest']['id'])
        expr=string.format(" %s %s dev %s root handle %s:0 htb default %s ", const_tc_qdisc, act, dev, flow_id, tmp_xq_id)
        table.insert(tblist,expr)

        -- filter for APP-XQ-comminucation, direct go out of htb root
        expr=string.format(" %s %s dev %s parent %s: prio %s handle %s fw classid %s:0 ",
                const_tc_filter, act, dev, flow_id, g_filter_prio_tab['PRIO_SUBNET_HIGH'], '0x00010000/0x000f0000', flow_id)
        table.insert(tblist,expr)
    end

    local quan_v=math.ceil(g_quantum_value * 5)
    expr = string.format(" %s %s dev %s parent %s: classid %s:%s htb rate %s%s quantum %d burst %d cburst %d", const_tc_class, act, dev, flow_id, flow_id, parent, ratelimit, UNIT, quan_v, buffer, cbuffer)
    table.insert(tblist,expr)

    -- 构建layer1的常驻队列结构
    for _name,_v in pairs(g_htb_layer1) do
        local tmp_id_str=dec2hexstr(_v['id'])
        local lrate=math.ceil(ratelimit*_v[direction]['rate']*g_host_rate_factor)
        local lceil=math.ceil(ratelimit*_v[direction]['ceil'])
        -- 对于guest，取配置的值
        if _name == 'guest' then
            local lceil_guest = 0
            if g_cur_guest_max_band[direction] <= 1 then
                lceil_guest = math.ceil(ratelimit*g_cur_guest_max_band[direction])
            else
                lceil_guest = math.ceil(g_cur_guest_max_band[direction])
            end
            lceil = lceil_guest
        end
        if lrate > lceil then lrate = lceil end
        buffer=math.ceil(lrate*1024/8.0/g_CONFIG_HZ*g_htb_buffer_factor)
        cbuffer=math.ceil(lceil*1024/8.0/g_CONFIG_HZ*g_htb_buffer_factor)
        quan_v = math.ceil(g_quantum_value*_v['quan'])
        expr = string.format(" %s %s dev %s parent %s:%s classid %s:%s htb rate %s%s ceil %s%s prio %s quantum %s burst %d cburst %d",
            const_tc_class, act, dev, flow_id, parent, flow_id, tmp_id_str, lrate, UNIT, lceil, UNIT, _v['prio'],quan_v, buffer, cbuffer)
        table.insert(tblist,expr)

        -- filter, only for add
        if act == 'add' then
            if _v['fwmark'] ~= '' then
                expr=string.format(" %s %s dev %s parent %s: prio %s handle %s fw classid %s:%s",
                        const_tc_filter, act, dev, flow_id, g_filter_prio_tab['PRIO_SUBNET_HIGH'], _v['fwmark'], flow_id, tmp_id_str)
                table.insert(tblist,expr)
            end
        end
    end

    -- 构建自定义优先级结构
    local customer_pid=g_htb_layer1['prio']['id']
    local customer_id_str=dec2hexstr(customer_pid)
    local customer_rate=g_htb_layer1['prio'][direction]['rate']
    local customer_ceil=g_htb_layer1['prio'][direction]['ceil']
    local high_class_id
    for _name,_v in pairs(g_htb_layer2_prio) do
        local tmp_id_str = dec2hexstr(customer_pid+_v['id'])
        local lrate=math.ceil(ratelimit*customer_rate*_v[direction]['rate']*g_host_rate_factor)
        local lceil=math.ceil(ratelimit*customer_ceil*_v[direction]['ceil'])
        buffer=math.ceil(lrate*1024/8.0/g_CONFIG_HZ*g_htb_buffer_factor)
        cbuffer=math.ceil(lceil*1024/8.0/g_CONFIG_HZ*g_htb_buffer_factor)
        quan_v = math.ceil(g_quantum_value*_v['quan'])
        expr = string.format(" %s %s dev %s parent %s:%s classid %s:%s htb rate %s%s ceil %s%s prio %s quantum %s burst %d cburst %d",
            const_tc_class, act, dev, flow_id, customer_id_str, flow_id, tmp_id_str, lrate, UNIT, lceil, UNIT, _v['prio'],quan_v, buffer, cbuffer)
        table.insert(tblist,expr)

        if _v['prio_packets'] == 1 and act == 'add' then
            high_class_id = tmp_id_str
            -- 将special包加入到高优先级队列
            if high_class_id ~= '' then
                tc_add_special_filter(tblist, act, dev, flow_id, direction, high_class_id)
            end
        end

        -- filter, only for add
        if act == 'add' and _v['fwmark'] ~= '' then
            expr=string.format(" %s %s dev %s parent %s: prio %s handle %s fw classid %s:%s",
                const_tc_filter, act, dev, flow_id, g_filter_prio_tab['PRIO_CUSTOM'], _v['fwmark'], flow_id, tmp_id_str)
            table.insert(tblist,expr)
        end
    end



    if not exec_cmd(tblist,nil) then
        logger(3,'ERROR: add root TC-layer1 qdisc rules failed.')
        system_exit()
        -- exit system when fatal error
    end
    return true
end

-- 构建layer2的htb-host结构
function tc_update_layer2_hosts_qdisc_class_filter(act, host_id, host_mode, dev, flow_id, rate, ceil, quantum)
    if host_id < 1 or host_id >= 255 then -- only support [1,255)
        logger(3,"fatal error, not supported host id: " .. host_id .." for layer2 htb host.")
        return false
    end

    local tblist={}
    local expr=''

    local parent_id_num = g_htb_layer1['host']['id']
    local parent_id_str=dec2hexstr(parent_id_num)
    local host_id_num = host_id*0x10
    local host_id_str=dec2hexstr(host_id_num)

    local host_base_num = parent_id_num + host_id*0x10                               -- 个体host的根
    local host_root_id = dec2hexstr(host_base_num)

    -- 个体host的根root-class, ID: flow_id:flow_id
    local buffer = math.ceil(tonumber(rate)*1024/8.0/g_CONFIG_HZ*g_htb_buffer_factor)
    local cbuffer = math.ceil(tonumber(ceil)*1024/8.0/g_CONFIG_HZ*g_htb_buffer_factor)
    if act == 'del' then
        expr = string.format(" %s %s dev %s classid %s:%s ", const_tc_class, act, dev, flow_id, host_root_id)
        table.insert(tblist,1,expr)    -- insert from backend
    elseif act == 'change' then
        expr = string.format(" %s %s dev %s parent %s:%s classid %s:%s htb rate %s%s ceil %s%s burst %d cburst %d quantum %d ", const_tc_class, act, dev, flow_id, parent_id_str, flow_id, host_root_id, rate, UNIT, ceil, UNIT, buffer, cbuffer, quantum)
        table.insert(tblist,1,expr)    -- change from leaf-node
    else -- add
        expr = string.format(" %s %s dev %s parent %s:%s classid %s:%s htb rate %s%s ceil %s%s burst %d cburst %d quantum %d ", const_tc_class, act, dev, flow_id, parent_id_str, flow_id, host_root_id, rate, UNIT, ceil, UNIT, buffer, cbuffer, quantum)
        table.insert(tblist,expr)  -- add
    end

    -- 个体host的流分类叶节点（包含最底层的sfq叶子）
    local last_handle
    local nSubs=#(host_mode['subclass'])
    for m=1, nSubs do
        local cls = g_htb_layer2_host[m]
        local id_class = dec2hexstr(host_id*0x10 + m)
        local lclassid = dec2hexstr(host_base_num + m)         -- 个体的流分类队列
        local class_value = host_id_num + m
        local class_value_str=dec2hexstr(class_value)
        local htb_prio = cls['prio']  -- htb的优先级必须在[1,7]之间
        if htb_prio > 7 or htb_prio <= 0 then htb_prio = 7 end

        if act == 'del' then
            expr = string.format(" %s %s dev %s classid %s:%s ", const_tc_class, act, dev, flow_id, lclassid)
            table.insert(tblist, 1, expr)
        else
            local lrate = math.ceil(rate * cls['rate'])
            local lceil = math.ceil(ceil * cls['ceil'])

            -- 流分类节点
            local buffer = math.ceil(tonumber(lrate)*1024/8.0/g_CONFIG_HZ*g_htb_buffer_factor)
            local cbuffer = math.ceil(tonumber(lceil)*1024/8.0/g_CONFIG_HZ*g_htb_buffer_factor)
            expr = string.format(" %s %s dev %s parent %s:%s classid %s:%s htb rate %s%s ceil %s%s prio %s  quantum %s burst %d cburst %d ", const_tc_class, act, dev, flow_id, host_root_id, flow_id, lclassid, lrate, UNIT, lceil, UNIT, htb_prio, quantum, buffer, cbuffer)
            if act == 'change' then
                table.insert(tblist,1,expr)
            else
                table.insert(tblist, expr)
            end
        end

        -- filter with fwmark 流分类filter $$ filter prio can be [1,N)
        -- 这里需要注意,fw的prio与1级filter有关系,同一prio拥有相同的filter,且相同的mask
        expr = string.format(" %s %s dev %s parent %s: prio %s handle 0x%s00000/0xfff00000 fw classid %s:%s ", const_tc_filter, act, dev, flow_id , g_filter_prio_tab['PRIO_HOST_AND_TYPE'], class_value_str, flow_id, lclassid)
        if act == 'del' then
            table.insert(tblist, 1, expr)
        elseif act == 'change' then
            -- filter is not changed.
        else
            table.insert(tblist,expr)
        end

        -- sfq leaf qdisc SFQ叶子节点,自动分类ID
        expr = string.format(" %s %s dev %s parent %s:%s sfq perturb 10 ", const_tc_qdisc, act, dev, flow_id, lclassid)
        if act == 'del' then
            table.insert(tblist, 1, expr)
        elseif act == 'change' then
            -- no change for sfq
        else
            table.insert(tblist, expr)
        end

        last_handle = lclassid
    end

    --default filter
    expr = string.format(" %s %s dev %s parent %s:  prio %s handle 0x%s00000/0xff000000 fw classid %s:%s ", const_tc_filter, act, dev, flow_id, g_filter_prio_tab['PRIO_HOST_ONLY_LOW'], host_id_str, flow_id, last_handle)
    if act == 'del' then
        table.insert(tblist, 1, expr)
    elseif act == 'change' then
        -- no change for filter
    else
        table.insert(tblist,expr)
    end

    if not exec_cmd(tblist,nil) then
        logger(3,'update [' .. act .. '] host_root_id ' .. host_root_id .. ' failed.')
        system_exit()
        return false
    end

    return true
end

-- 删除host节点下所有的stations
function delete_all_hosts_qos(dev, direction, flow_id)
    local act='del'
    local mode=mode_def[cfg_default_mode]
    local host_id_base=g_htb_layer1['host']['id']
    logger(3,'=== clear htb layer2 host ' .. direction .. ' rules.')
    for _ip,_v in pairs(g_crt_ipmac_map or {}) do
        local host_id = tonumber(_v['id'])
        if not tc_update_layer2_hosts_qdisc_class_filter(act, host_id, mode,dev,flow_id, 0,0,0) then
            return false
        end
    end
    return true
end

-- 更新host节点下所有的stations
function refresh_all_hosts_qos(dev, direction, flow_id, rate_host, ceil_host)

    -- 当前仅仅支持一种mode格式
    local mode=mode_def[cfg_default_mode]
    local act
    local host_id_base=g_htb_layer1['host']['id']

    -- step1.删除需要删除的IP记录
    act = 'del'
    for ip,v in pairs(g_crt_ipmac_map or {}) do
        -- logger(3,'loop g_crt_ipmac_map,'..v['mac']..','..ip)
        if v['st'] == 'S_DEL' then
            logger(3,'--- del MAC ' .. v['mac'] .. ', IP ' .. ip)
            local host_id = tonumber(v['id'])

            -- 删除过期的S_DEL 记录
            if not tc_update_layer2_hosts_qdisc_class_filter(act, host_id, mode,dev,flow_id, 0,0,0) then
                return false
            end
        end
    end

    -- step2.新增或更新的IP记录,以IP为KEY
    for ip,v in pairs(g_nxt_ipmac_map or {}) do
        local host_act = v['st']

        if host_act == 'S_NEW' then act = 'add'
        elseif host_act == 'S_UPD' then act = 'change'
        else act = 'del'end

        local mac = v['mac']
        if not g_alive_hosts[mac] then
            logger(3,'exception case')
            return false
        end

        -- 获取此IP对应的设置参数
        local ip_group = group_def[mac] or group_def[cfg_default_group]

        local lhost_rate,lhost_ceil = 0,0

        if direction == UP then
            if ip_group['each_up_rate'] > 0 then lhost_rate = ip_group['each_up_rate'] end
            if ip_group['each_up_ceil'] > 1 then
                lhost_ceil = ip_group['each_up_ceil']
            else
                lhost_ceil = ceil_host;
            end
        else
            if ip_group['each_down_rate'] > 0 then lhost_rate = ip_group['each_down_rate'] end
            if ip_group['each_down_ceil'] > 1 then
                lhost_ceil = ip_group['each_down_ceil']
            else
                lhost_ceil = ceil_host;
            end
        end

        local in_ceil = math.ceil(lhost_ceil)
        local in_rate = math.ceil(lhost_rate * rate_host)
        local ratio = ip_group['min_grp_uplink']*10
        if ratio <= 0 then ratio =1 end
        if ratio > 10 then ratio =10 end
        local in_quan = math.ceil(g_quantum_value * ratio)

        -- less than 1kb/s(8kbps), will regard it as no-limit
        if in_ceil < 8 then in_ceil = rate_host end
        if in_rate > in_ceil then in_rate = in_ceil end
        if in_rate < 8 then in_rate = 8 end

        logger(3, '+++ MAC ' .. mac .. ",IP " .. ip .. ', ' .. direction .. ',' .. in_rate .. '-' .. in_ceil .. ', id:' .. v['id'] .. ',action:' .. host_act)

        --new tc class for host
        local host_base_id = tonumber(v['id'])
        local parent_id = g_htb_layer1['host'][direction]['id']
        if not tc_update_layer2_hosts_qdisc_class_filter(act, host_base_id, mode, dev, flow_id, in_rate, in_ceil, in_quan) then
            return false
        end
    end

    return true
end

function update_flow_type()
    local flow_type={
        {root= '1', dev = g_ifb, direction = DOWN, },
        {root= '2', dev = g_wan_if, direction = UP, },
    }
    g_htb_major_id={DOWN='1',UP='2'}

    return flow_type
end

function flush_prio_layer1()
    local act='add'
    if g_prio_layer1_ready then act='replace' end
    logger(3,'======== Flush QoS TC-layer1 Prio ' .. act ..' ====band: Up:' .. cur_bandwidth[UP] .. 'kbps,Down: ' .. cur_bandwidth[DOWN] .. 'kbps ========')
    for seq,flow_item in pairs(g_flow_type) do
        local dir=flow_item['direction']
        local bandwidth = cur_bandwidth[dir]
        tc_fixed_layer1_prio_root_qdisc_class_filters(flow_item['dev'], flow_item['root'], act, bandwidth, flow_item['direction'])
    end
    g_prio_layer1_ready = true
end

--刷新root层级的结构
function flush_htb_layer1()
    local act='add'
    if g_htb_layer1_ready then act = 'change' end
    if act ~= 'change' and act ~= 'add' then
        logger(3,'do not support such act: ' .. act .. ' for htb layer1.')
        return false
    end

    logger(3,'======== Flush QoS TC-layer1-htb ' .. act ..' ====band: Up:' .. cur_bandwidth[UP] .. 'kbps,Down: ' .. cur_bandwidth[DOWN] .. 'kbps ========')
    for seq,flow_item in pairs(g_flow_type) do
        local dir=flow_item['direction']
        local bandwidth = cur_bandwidth[dir]
        tc_fixed_layer1_htb_root_qdisc_class_filters(flow_item['dev'], flow_item['root'], act, bandwidth, flow_item['direction'])
    end
    g_htb_layer1_ready = true
end

-- 刷新host层级的结构
function flush_htb_layer2()

    logger(3,'======== Flush QoS TC-layer2-host ====band: Up:' .. cur_bandwidth[UP] .. 'kbps,Down: ' .. cur_bandwidth[DOWN] .. 'kbps ========')
    for seq,flow_item in pairs(g_flow_type) do
        local dir=flow_item['direction']
        local bandwidth = cur_bandwidth[dir]
        local host_rate,host_ceil=g_htb_layer1['host'][dir]['rate'],g_htb_layer1['host'][dir]['ceil']
        local lrate,lceil = math.ceil(bandwidth* host_rate), math.ceil(bandwidth*host_ceil)
        if not refresh_all_hosts_qos(flow_item['dev'], dir, flow_item['root'], lrate, lceil ) then
            return false
        end
    end

    -- 切换ipmac状态信息表
    g_crt_ipmac_map = nil
    g_crt_ipmac_map = copytab(g_nxt_ipmac_map)
    g_nxt_ipmac_map = nil

    return true
end

--刷新两层规则
function flush_qos_rules(status_changed)

    -- 如果band太小，则不设置任何规则，回归到0状态
    if cur_bandwidth[UP] < 1 or cur_bandwidth[DOWN] < 1 then
        return false
    end

    if cfg_qos_enabled == '0' then
        -- prio算法当道，1状态，为high-priority更新host列表
        update_host_list_compact()
        if status_changed then
            flush_prio_layer1()
            return true
        end
    else
        -- htb算法当道，2状态
        if status_changed then
            -- 状态变化，先更新htb layer1
            flush_htb_layer1()
        end

        -- htb检查host是否变化
        local host_changed = check_host_changed()

        --以下是htb的layer2
        if cfg_qos_enabled ~= cur_qos_enabled then   -- apply mark for qos flag from 0 -> 1
            ipt_mark_hosts_nf()
        end

        if status_changed or host_changed then
            -- 为每个host重新分配顶层带宽
            local host_counter_in_total = arrange_bandwidth()
            flush_htb_layer2()
        end
    end

    return true
end

--根据实时wan口速率记录band带宽
function update_rec_band()
    local _rec_bandwidth={UP=0,DOWN=0}
    local ret = g_ubus:call("trafficd", "wan", {})
    if ret then
        if ret and ret.max_rx_rate and ret.max_tx_rate then
            local up,down = math.ceil(tonumber(ret.max_tx_rate)*8/1024.0),math.ceil(tonumber(ret.max_rx_rate)*8/1024.0)
            if _rec_bandwidth[UP] < up then
                _rec_bandwidth[UP] = up
            end
            if _rec_bandwidth[DOWN] < down then
                _rec_bandwidth[DOWN] = down
            end
        end
    end
    return _rec_bandwidth
end

-- 在update qos完成之后更新高优先级定义的filter
function update_QOS_high_prio_filter()
    -- stop状态下，不更新规则
    if g_QOS_Status == 0 then
        return
    end
    if g_high_prio_host_changed then
        g_high_prio_host_changed = false

        g_flow_type = update_flow_type()
        local is_htb = true
        if cur_qos_enabled == '0' then
            is_htb = false
        end

        for seq,flow_item in pairs(g_flow_type) do
            tc_update_host_prio_and_limit_filter(flow_item['dev'], flow_item['root'], is_htb)
        end
    end
end

-----------------------------------------------------------------------------
--  run QOS
-----------------------------------------------------------------------------
function update_QOS()

    -- 如果ifb状态未UP,则尝试UP
    if g_ifb_status ~= UP then
        --check if dev ifb0 is up
        local ifb_up = '/usr/sbin/ip link set ' .. g_ifb .. ' up '
        local ifb_check = '/usr/sbin/ip link show ' .. g_ifb .. ' |grep "state DOWN"'
        local ret = util.exec(ifb_up)
        ret = util.exec(ifb_check)
        if ret == '' then
            g_ifb_status = UP
        else
            logger(3, 'ifb0 is not up, wait for next link up.')
            return false    -- 继续下一次set link up
        end
    end

    g_flow_type = update_flow_type()

    -- 读取配置文件
    if not read_qos_config() then
        logger(3,'read config failed, exit!')
        return false
    end

    local status_changed = check_status_changed()

    -- 状态2下
    if g_QOS_Status == 2 then
        -- to exit
        if g_close_layer1  == 1 then
            -- 设置状态为OFF,并且清除所有规则
            g_QOS_Status = 0
            cur_qos_enabled = '0'
            cleanup_system()
            return true
        end

        -- to close htb
        if cfg_qos_enabled == '0' then  -- htb -> prio
            cleanup_system()
            if not flush_qos_rules(status_changed) then
                cleanup_system()
                logger(3, 'Update QoS rules failed, cleanup QOS.')
                cur_qos_enabled='0'
                g_QOS_Status = 0
                return false
            end
            g_QOS_Status = 1
            cur_qos_enabled = '0'
            return true
        else  -- status 2 cycle
            if not flush_qos_rules(status_changed) then
                cleanup_system()
                logger(3, 'Update QoS rules failed, cleanup QOS.')
                cur_qos_enabled='0'
                g_QOS_Status = 0
                return false
            end
            cur_qos_enabled = '1'
        end
        return true
    end

    -- 状态1下
    if g_QOS_Status == 1 then
        -- to close deamon
        if g_close_layer1  == 1 then  -- close prio, status 1 -> 0
            g_QOS_Status = 0
            cur_qos_enabled = '0'
            cleanup_system()    -- clear prio
            return true
        end

        if cfg_qos_enabled == '0' then   -- prio -> prio
            flush_qos_rules(status_changed)
            cur_qos_enabled = '0'
            return true
        else  -- open htb, prio -> htb
            cleanup_system()
            if not flush_qos_rules(status_changed) then
                cleanup_htb_layer2_host()
                logger(3, 'Update QoS rules failed, cleanup QOS.')
                cur_qos_enabled='0'
                g_QOS_Status = 0
                return true
            end
            g_QOS_Status = 2     -- QoS TC-HTB 已经开启
            cur_qos_enabled = '1'
            return true
        end
        return true
    end

    -- 状态0下，未设置顶层prio
    if g_QOS_Status == 0 then
        if g_close_layer1  ~= 1 then   -- open prio, status 0 -> 1
            if cfg_qos_enabled == '0' then
                if not flush_qos_rules(true) then   -- NULL -> prio
                    --cleanup_system()
                    cur_qos_enabled='0'
                    g_QOS_Status = 0
                    return true
                end
                g_QOS_Status = 1
                cur_qos_enabled = '0'
                return true
            end

            if cfg_qos_enabled == '1' then
                if not flush_qos_rules(true) then   -- NULL -> htb
                    cleanup_system()
                    cur_qos_enabled='0'
                    g_QOS_Status = 0
                    return true
                end
                g_QOS_Status = 2
                cur_qos_enabled = '1'
                return true
            end

        else
            return true
        end
    end

    return true
end

-----------------------------------------------------------------------------
--  get tc limit counters
-----------------------------------------------------------------------------
function update_tc_counters()

    if cur_qos_enabled == '0' then
        return
    end

    local up_id=g_htb_major_id[UP]
    local down_id=g_htb_major_id[DOWN]

    local tc_counters={
            [up_id]={},   -- uplink
            [down_id]={},   -- downlink
    }
    for k,v in pairs({g_wan_if, g_lan_if}) do
        local w={}
        local pp=io.popen(string.format(const_tc_dump, v))
        local data=pp:read("*line")
        local lineno=1

        while data do
            -- 1st line
            local first,_,ldir,lclass,lrate,lceil =  string.find(data,"class htb (%d+):(%w+).*rate (%w+) ceil (%w+)")
            if first then
                if not tc_counters[ldir][lclass] then
                    tc_counters[ldir][lclass] = {}
                end

                tc_counters[ldir][lclass]['r'] = lrate
                tc_counters[ldir][lclass]['c'] = lceil
            end
            data = pp:read('*line')
        end
        pp:close()
    end

    -- 清空先
    g_limit={}
    local maxup, maxdown, minup, mindown= 0, 0, 0, 0
    local tmp_id
    local host_id_base = g_htb_layer1['host']['id']
    for k,v in pairs(g_crt_ipmac_map) do
        tmp_id = dec2hexstr(host_id_base + v['id']*0x10)
        if tc_counters[up_id][tmp_id] then
            maxup = tc_counters[up_id][tmp_id]['c']
            minup = tc_counters[up_id][tmp_id]['r']
        end

        if tc_counters[down_id][tmp_id] then
            maxdown = tc_counters[down_id][tmp_id]['c']
            mindown = tc_counters[down_id][tmp_id]['r']
        end

        local up,down
        local mac = v['mac']
        if mac and group_def[mac] then
            up={max_per=group_def[mac]['max_grp_uplink'], min_per=group_def[mac]['min_grp_uplink'], max_cfg=math.ceil(group_def[mac]['max_grp_uplink']),max=maxup, min_cfg=math.ceil(group_def[mac]['min_grp_uplink']*cfg_bandwidth[UP]), min=minup}
            down={max_per=group_def[mac]['max_grp_downlink'], min_per=group_def[mac]['min_grp_downlink'], max_cfg=math.ceil(group_def[mac]['max_grp_downlink']), max=maxdown,min_cfg=math.ceil(group_def[mac]['min_grp_downlink']*cfg_bandwidth[DOWN]),min=mindown}
        else
            up={max_per=0,min_per=0.5,max_cfg=0, max=maxup,min_cfg=0, min=minup}
            down={max_per=0,min_per=0.5,max_cfg=0, max=maxdown,min_cfg=0,min=mindown}
        end
        g_limit[k]={MAC=mac,UP=up,DOWN=down}
    end

end

-- 简化配置中MAC设置
function compact_output_group_def()
    local ret={}
    local t={'max_grp_uplink','min_grp_uplink','max_grp_downlink','min_grp_downlink'}
    for k, v in pairs(group_def) do
        if k ~= cfg_default_group then
            ret[k]={}
            for _,n in pairs(t) do
                ret[k][n]=v[n]
            end
        end
    end
    return ret
end

function uci_commit_save(flag)
    if flag then
        g_cursor:commit('miqos')

        -- tmp下的配置改变,复写回/etc下
        if not tmp2cfg() then
            logger(1, 'copy tmp cfg to /etc/config/ failed.')
        end
    end
end

-----------------------------------------------------------------------------
--  configuration operation, add/chg, del, update
-----------------------------------------------------------------------------
function add_or_change_group(mac, maxup, maxdown,minup,mindown, todisk)

    local str_mac=string.upper(mac)
    local mac_name=string.gsub(str_mac,':','')
    local all=g_cursor:get_all('miqos')
    local name = ''
    for k,v in pairs(all) do
        if v['.type'] == 'group' and v['name'] == str_mac then
            name = k
            break
        end
    end

    if name == '' then
        name = g_cursor:section('miqos','group',mac_name)
        g_cursor:set('miqos',name,'name',str_mac)
        g_cursor:set('miqos',name,'min_grp_uplink','0.5')
        g_cursor:set('miqos',name,'min_grp_downlink','0.5')
        g_cursor:set('miqos',name,'max_grp_uplink','0')
        g_cursor:set('miqos',name,'max_grp_downlink','0')
        g_cursor:set('miqos',name,'mode','general')
        g_cursor:set('miqos',name,'mac',{str_mac})
    end

    local tmp_num
    if minup then
        tmp_num = tonumber(minup)
        if tmp_num <= 0 or tmp_num > 1 then
            minup = g_default_min_updown_factor
            if g_debug then
                logger(3,'setting min reserve out of range, set it to default value.')
            end
        end
        g_cursor:set('miqos',name,'min_grp_uplink',minup)
    end
    if mindown then
        tmp_num = tonumber(mindown)
        if tmp_num <= 0 or tmp_num > 1 then
            mindown = g_default_min_updown_factor
            if g_debug then
                logger(3,'setting min reserve out of range, set it to default value.')
            end
        end
        g_cursor:set('miqos',name,'min_grp_downlink',mindown)
    end
    if maxup then
        tmp_num = tonumber(maxup)
        if tmp_num < 8 then
            maxup = 0
            if g_debug then
                logger(3,'NOTE: setting min reserve out of range, set it to default value.')
            end
        end
        g_cursor:set('miqos',name,'max_grp_uplink',maxup)
    end
    if maxdown then
        tmp_num = tonumber(maxdown)
        if tmp_num < 8 then
            maxdown = 0
            if g_debug then
                logger(3,'NOTE: setting min reserve out of range, set it to default value.')
            end
        end
        g_cursor:set('miqos',name,'max_grp_downlink',maxdown)
    end

    uci_commit_save(todisk)
end

function update_qos_auto_mode(mode, todisk)

    g_cursor:set('miqos','settings','qos_auto',mode)

    uci_commit_save(todisk)
end

function del_group(mac, todisk)

    local all=g_cursor:get_all('miqos')

    if mac then
        local str_mac = string.upper(mac)
        for k,v in pairs(all) do
            if v['.type'] == 'group' and v['name'] == str_mac then
                g_cursor:delete('miqos',k)
                break
            end
        end
    else
        for k,v in pairs(all) do
            if v['.type'] == 'group' and v['name'] ~= '00' then
                g_cursor:delete('miqos',k)
            end
        end
    end

    uci_commit_save(todisk)
end

function update_bw(max_up,max_down, todisk)

    if tonumber(max_up) >= 0 and tonumber(max_down) >= 0 then
        g_cursor:set('miqos','settings','upload',max_up)
        g_cursor:set('miqos','settings','download',max_down)

        uci_commit_save(todisk)
        return true
    end
    return false
end

function update_qos_enabled(qos_on_flag)
    if qos_on_flag then
        g_cursor:set('miqos','settings','enabled','1')
    else
        g_cursor:set('miqos','settings','enabled','0')
    end

    g_cursor:commit('miqos')
end

function update_guest_percent(in_up,in_down,todisk)
    if in_up == 0 then in_up = g_guest_default_band_factor end
    if in_down == 0 then in_down= g_guest_default_band_factor end

    g_cursor:set('miqos','guest','up_per',in_up)
    g_cursor:set('miqos','guest','down_per',in_down)

    uci_commit_save(todisk)
    return true
end

function get_guest_percent()
    local up,down
    up = g_cursor:get('miqos','guest','up_per') or g_guest_default_band
    down = g_cursor:get('miqos','guest','down_per') or g_guest_default_band
    return tonumber(up),tonumber(down)
end

function get_bw()
    local up = g_cursor:get('miqos','settings','upload') or '0'
    local down=g_cursor:get('miqos','settings','download') or '0'
    return up,down
end

function get_uptime()
    local pp=io.open('/proc/uptime')
    local n=pp:read('*n')
    return math.ceil(n)
end

-----------------------------------------------------------------------------
--  loop work for QOS
-----------------------------------------------------------------------------
function main_loop()

    local server=assert(socket.bind(cfg_host,cfg_port))
    server:settimeout(1)

    local now_time = get_uptime()
    local next_qos_time = now_time
    local delta
    local gc_timer=0

    -- tables for select event
    local set=newset()
    set:insert(server)    -- add 'server' into select events

    while true do

        now_time = get_uptime()       -- 读取当前的uptime ticks
        if now_time >= next_qos_time then
            if update_QOS() then
                gc_timer = gc_timer + 1
                if gc_timer >= 30 then
                    gc_timer = 0
                    local tmp_cnt = collectgarbage('count')
                    logger(3, "current mode:: --> " .. g_QOS_Status .. ',QoS-Mode:' .. QOS_TYPE .. ', LUA GC counter: ' .. tmp_cnt .. ', Bandwidth: U:'..tc_bandwidth[UP]..'kbps,D:'..tc_bandwidth[DOWN]..'kbps')
                end
                if g_debug then
                    -- logger(3, "current mode: --> " .. g_QOS_Status.. ', rec_speed: U:'..rec_bandwidth[UP]..'kbps,D:'..rec_bandwidth[DOWN] .. 'kbps')
                end
                -- 更新tc的counters,便于直接调用返回
                update_tc_counters()
            end

            update_QOS_high_prio_filter()       -- 更新高优先级的filter设置

            next_qos_time = now_time + cfg_qos_check_interval      -- 更新下一次update QOS检测时间
        end

        delta = next_qos_time - now_time
        -- logger(3,"TIME:" .. delta .. ' = '.. next_qos_time .. '-' .. now_time)
        if delta > cfg_qos_check_interval then
            logger(3, "Warning!!! plz check Update QoS delta = " .. delta .. ", it's too long!!!!")
            delta = cfg_qos_check_interval
        end

        local readable, _, error = socket.select(set, nil , delta)
        for _,v in ipairs(readable) do

            if v == server then
                -- logger(3, 'new client come in ...')

                local clt=v:accept()
                if clt then
                    clt:settimeout(1)
                    set:insert(clt)
                else
                    logger(3, 'accept client error.')
                end
            else
                local data,error = v:receive()

                if error then
                    v:close()
                    -- logger(3, 'client is disconnected.')
                    set:remove(v)
                else
                    local args=string.split(data,' ')
                    if not args[1] then
                        v:send(json.encode({status=3}))
                    else
                        if args[1] == 'on' then
                            -- uci on QoS TC-HTB  done before send on command
                            next_qos_time = 0
                            g_close_layer1  = 0  -- keep prio and htb on
                            read_network_conf()     --update network conf when command triggered
                            update_qos_enabled(true)
                            g_high_prio_host_changed = true     -- force to refresh
                            v:send(json.encode({status=0}))
                            logger(3,'======== COMMAND `qos on`============')
                        elseif args[1] == 'off' then
                            next_qos_time = 0
                            g_close_layer1 = 0   -- keep prio, but clean htb
                            read_network_conf()     --update network conf when command triggered
                            update_qos_enabled(false)
                            g_high_prio_host_changed = true     -- force to refresh
                            v:send(json.encode({status=0}))
                            logger(3,'======== COMMAND `qos off` ============')
                        elseif args[1] == 'shutdown' then
                            g_close_layer1  = 1   -- clean prio and htb
                            next_qos_time = 0
                            v:send(json.encode({status=0}))
                            logger(3,'======== COMMAND `qos shutdown` ============')
                        elseif args[1] == 'die' then
                            v:send(json.encode({status=0}) .. "\n")
                            v:close()
                            logger(3,'======== COMMAND `qos die` ============')
                            system_exit()    -- exit system
                        elseif args[1] == 'nprio' then      -- 加入全host优先级定制
                            -- args[2]: 'add/del'
                            -- args[3]: ip
                            -- args[4]: HIGH_PRIO_WITHOUT_LIMIT/HIGH_PRIO_WITH_BANDLIMIT
                            if args[2] ~= 'add' and args[2] ~= 'del' then
                                v:send(json.encode({status=1,data="only support add/del op."}))
                            else
                                if args[4] and args[4]=='HIGH_PRIO_WITHOUT_LIMIT' then
                                    tc_update_host_prio_and_limit(args[2], args[3],const_NO_LIMIT_HIGH);
                                elseif args[4] and args[4]=='HIGH_PRIO_WITH_BANDLIMIT' then
                                    tc_update_host_prio_and_limit(args[2], args[3],const_BAND_LIMIT_HIGH);
                                else
                                    logger(3, 'not supported new prio change.' .. args[1] .. ' ' .. args[3] .. ' ' .. args[4])
                                end
                                v:send(json.encode({status=0}))
                            end

                        elseif args[1] == 'change_band' then
                            if args[2] and args[3] then
                                if update_bw(args[2],args[3],true) then
                                    v:send(json.encode({status=0}))
                                    cfg_changed = 'ALL'
                                    next_qos_time = 0
                                    logger(3,'======== COMMAND `qos change_band ` =====')
                                else
                                    v:send(json.encode({status=3,data='parameter foramt error.'}))
                                end
                            else
                                v:send(json.encode({status=1, data='parameter lost'}))
                            end

                        elseif args[1] == 'show_band' then     -- get current setting band-width
                            local u,d = get_bw()
                            v:send(json.encode({status=0,data={uplink=u,downlink=d}}))

                        elseif args[1] == 'on_guest' then   -- qos off时，依然可以设置guest限制
                            if args[2] and args[3] then
                                local up_p,down_p = tonumber(args[2]),tonumber(args[3])
                                update_guest_percent(up_p,down_p,true)
                                next_qos_time = 0
                                cfg_changed = 'ALL'
                                v:send(json.encode({status=1,data='ok'}))
                            else
                                v:send(json.encode({status=2,data='parameter wrong.'}))
                            end

                        else

                            if cur_qos_enabled == '0' then
                                v:send(json.encode({status=4,data='qos disabled.'}))
                            else

                                if args[1] == 'show_limit' then
                                    local ret={}
                                    -- 在每次更新之后进行调用
                                    -- local g_limit = update_tc_counters()
                                    if args[2] then
                                        local mac=string.upper(args[2])

                                        if g_limit[args[2]] then
                                            ret[args[2]] = g_limit[args[2]]
                                            v:send(json.encode({status=0,mode=QOS_TYPE,bandwidth={upload=tc_bandwidth[UP],download=tc_bandwidth[DOWN]},data=ret}))
                                        else
                                            ret[args[2]] = {status=3,data='not exist ip/device.'}
                                            v:send(json.encode({status=3,data='not exist ip/device.'}))
                                        end
                                    else
                                        v:send(json.encode({status=0,mode=QOS_TYPE,bandwidth={upload=tc_bandwidth[UP],download=tc_bandwidth[DOWN]},data=g_limit}))
                                    end
                                elseif args[1] == 'show_cfg' then
                                    -- 返回配置里所有MAC对应的auto,min,max,both模式下的配置数据
                                    local ret={status=0,data=compact_output_group_def(),mode=QOS_TYPE }
                                    v:send(json.encode(ret))
                                elseif args[1] == 'on_limit' or args[1] == 'set_limit' then    -- add/modify cfg
                                    local result={status=0}
                                    if not args[2] or not args[3] then
                                        result = {status=1,data='limit type + mac parameter lost.'}
                                    else
                                        local active_soon = false
                                        if args[1] == 'on_limit' then
                                            active_soon = true
                                            cfg_changed, next_qos_time = 'HOST',0
                                        end

                                        --here commented temporarily and open it with UI
                                        if args[2] == 'max' then
                                            add_or_change_group(args[3],args[4],args[5],nil,nil,active_soon)
                                        elseif args[2] == 'min' then
                                            add_or_change_group(args[3],nil,nil,args[4],args[5],active_soon)
                                        elseif args[2] == 'both' then
                                            add_or_change_group(args[3],args[4],args[5],args[6],args[7],active_soon)
                                        else
                                            cfg_changed = 'NONE'
                                            result = {status=1,data='not supported limit type.'}
                                        end
                                    end
                                    v:send(json.encode(result))
                                    logger(3,'======== COMMAND `qos limit on` ============')

                                elseif args[1] == 'off_limit' or args[1] == 'reset_limit' then
                                    local tobe_del = args[2]
                                    if args[1] == 'off_limit' then
                                        cfg_changed = 'HOST'
                                        next_qos_time = 0
                                        del_group(tobe_del, true)
                                    else
                                        del_group(tobe_del, false)
                                    end
                                    v:send(json.encode({status=0}))
                                    logger(3,'======== COMMAND `qos limit off x` ============')

                                elseif args[1] == 'set_type' then
                                    if args[2] then
                                        if args[2] == 'auto' then
                                            logger(3, "----->>set to auto-limit-mode.")
                                            QOS_TYPE= 'auto'
                                        elseif args[2] == 'min' then
                                            logger(3, "----->>set to min-limit-mode.")
                                            QOS_TYPE = 'min'
                                        elseif args[2] == 'max' then
                                            logger(3, "----->>set to max-limit-mode.")
                                            QOS_TYPE = 'max'
                                        elseif args[2] == 'both' then
                                            logger(3, "----->>set to both-limit-mode.")
                                            QOS_TYPE = 'both'
                                        end
                                    else
                                        QOS_TYPE = 'auto'
                                    end

                                    update_qos_auto_mode(QOS_TYPE, true)
                                    cfg_changed = 'HOST'
                                    next_qos_time = 0
                                    v:send(json.encode({status=0}))

                                elseif args[1] == 'apply' then
                                    -- 保存并使生效
                                    uci_commit_save(true)
                                    if args[2] and args[2] == 'both' then
                                        cfg_changed='ALL'
                                    else
                                        cfg_changed='HOST'
                                    end
                                    next_qos_time = 0
                                    v:send(json.encode({status=0}))

                                else
                                    v:send(json.encode({status=2,data='Not supported command.'}))
                                end
                            end
                        end
                    end

                    v:send('\n')
                    v:close()
                    set:remove(v)
                end
            end
        end
    end
end


--[[main]]-------------------
function main()

    if system_init() then
        local s, e = pcall(function() main_loop() end)
        if not s
        then
            logger(3,e)
            cleanup_system()
        end
    else
        logger(3, 'system initial failed. exit.')
    end
end

main()

--[[main end]]-------------------
