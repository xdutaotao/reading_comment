/*
 * weighttp - a lightweight and simple webserver benchmarking tool
 *
 * Author:
 *     Copyright (c) 2009-2011 Thomas Porzelt
 *
 * License:
 *     MIT, see COPYING file
 */

/* client 结构定义 */
struct Client {
	enum {
		CLIENT_START,       /* 开始 */
		CLIENT_CONNECTING,  /* 调用connect，但是并未返回已连接 */
		CLIENT_WRITING,     /* 已连接，开始写 */
		CLIENT_READING,     /* 已写完，开始读 */
		CLIENT_ERROR,       /* 出现错误 */
		CLIENT_END          /* client 结束*/
	} state;

	enum {
		PARSER_START,       /* 解析过程开始 */
		PARSER_HEADER,      /* 解析头 */
		PARSER_BODY         /* 解析body*/
	} parser_state;

	Worker *worker;         /* client里包含众多worker */
	ev_io sock_watcher;     /* ev_io 记录fd和需要监听的event,以及事件的回调函数 */
	uint32_t buffer_offset; /* 接收缓存的偏移 */
	uint32_t parser_offset; /* 解析缓存的偏移*/
	uint32_t request_offset;    /* 请求的偏移*/
	ev_tstamp ts_start;     /* 开始和结束时间*/
	ev_tstamp ts_end;
	uint8_t keepalive;      /* 此client是否keepalive*/
	uint8_t success;
	uint8_t status_success;
	uint8_t chunked;
	int64_t chunk_size;
	int64_t chunk_received;
	int64_t content_length;
	uint64_t bytes_received; /* including http header */
	uint16_t header_size;

	char buffer[CLIENT_BUFFER_SIZE];    /* client buffer 32k */
};

Client *client_new(Worker *worker);
void client_free(Client *client);
void client_state_machine(Client *client);
