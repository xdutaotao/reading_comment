/*
 * weighttp - a lightweight and simple webserver benchmarking tool
 *
 * Author:
 *     Copyright (c) 2009-2011 Thomas Porzelt
 *
 * License:
 *     MIT, see COPYING file
 */

#include "weighttp.h"

static uint8_t client_parse(Client *client, int size);
static void client_io_cb(struct ev_loop *loop, ev_io *w, int revents);
static void client_set_events(Client *client, int events);
/*
static void client_add_events(Client *client, int events);
static void client_rem_events(Client *client, int events);

static void client_add_events(Client *client, int events) {
	struct ev_loop *loop = client->worker->loop;
	ev_io *watcher = &client->sock_watcher;

	if ((watcher->events & events) == events)
		return;

	ev_io_stop(loop, watcher);
	ev_io_set(watcher, watcher->fd, watcher->events | events);
	ev_io_start(loop, watcher);
}

static void client_rem_events(Client *client, int events) {
	struct ev_loop *loop = client->worker->loop;
	ev_io *watcher = &client->sock_watcher;

	if (0 == (watcher->events & events))
		return;

	ev_io_stop(loop, watcher);
	ev_io_set(watcher, watcher->fd, watcher->events & ~events);
	ev_io_start(loop, watcher);
}
*/

static void client_set_events(Client *client, int events) {
	struct ev_loop *loop = client->worker->loop;    /* 首先获取ev_loop */
	ev_io *watcher = &client->sock_watcher;         /* 获得client的ev_io */

	if (events == (watcher->events & (EV_READ | EV_WRITE)))     /* 如果ev_io的事件包含读写事件，则直接返回，不用设置了*/
		return;

	ev_io_stop(loop, watcher);  /* 把此ev_io从ev_loop上卸载先*/
	ev_io_set(watcher, watcher->fd, (watcher->events & ~(EV_READ | EV_WRITE)) | events); /* 清除读写事件，然后设置event */
	ev_io_start(loop, watcher); /* 再把ev_io挂回ev_loop上 */
}

/* 新建client, 传入参数worker*/
Client *client_new(Worker *worker) {
	Client *client;

	client = W_MALLOC(Client, 1);   /* 分配 1个Client大小内存*/
	client->state = CLIENT_START;   /* 设置client为START状态，便于状态流转 */
	client->worker = worker;
	client->sock_watcher.fd = -1;   /* 监听句柄复位*/
	client->sock_watcher.data = client; /* ev_io 的data参数设置为本client指针 */
	client->content_length = -1;        /* content length, buffer/request offset 均设置为0 */
	client->buffer_offset = 0;
	client->request_offset = 0;
	client->keepalive = client->worker->config->keep_alive;     /* 从client的worker处获取配置的keepalive*/
	client->chunked = 0;
	client->chunk_size = -1;
	client->chunk_received = 0;

	return client;
}

void client_free(Client *client) {  /* 释放client */
	if (client->sock_watcher.fd != -1) {    /* ev_io的监听句柄有效*/
		ev_io_stop(client->worker->loop, &client->sock_watcher);    /* 从ev_loop上吧ev_io watcher移除 */
		shutdown(client->sock_watcher.fd, SHUT_WR); /* 关闭 监听的句柄，一般是socket */
		close(client->sock_watcher.fd); /* 关闭句柄 */
	}

	free(client);   /* 释放client的内存 */
}

static void client_reset(Client *client) {  /* 这是client reset，不是init */
	//printf("keep alive: %d\n", client->keepalive);
	if (!client->keepalive) {   /* 如果不是keepalive，则先关闭掉fd */
		if (client->sock_watcher.fd != -1) {
			ev_io_stop(client->worker->loop, &client->sock_watcher);
			shutdown(client->sock_watcher.fd, SHUT_WR);
			close(client->sock_watcher.fd);
			client->sock_watcher.fd = -1;
		}

		client->state = CLIENT_START;
	} else {    /* 否则设置client为可写, 直接重复利用*/
		client_set_events(client, EV_WRITE);
		client->state = CLIENT_WRITING; /* 利用已经有的连接,所以状态为写状态*/
		client->worker->stats.req_started++;
	}

    /* 一干属性重置 */
	client->parser_state = PARSER_START;
	client->buffer_offset = 0;
	client->parser_offset = 0;
	client->request_offset = 0;
	client->ts_start = 0;
	client->ts_end = 0;
	client->status_success = 0;
	client->success = 0;
	client->content_length = -1;
	client->bytes_received = 0;
	client->header_size = 0;
	client->keepalive = client->worker->config->keep_alive;
	client->chunked = 0;
	client->chunk_size = -1;
	client->chunk_received = 0;
}

/* client 连接 */
static uint8_t client_connect(Client *client) {
	//printf("connecting...\n");
	start:

    /* 直接调用connect(fd,srv_addr,len(srv_addr)),
    这里需要处理两种情况，
    一种是connect返回正确，
    一种是立即返回错误，
    如果error-code是in-progress/already,表示正在连接，
    如果error-code是EINTR,再试一次，
    如果是is_conn表示已经连接成功
    */
	if (-1 == connect(client->sock_watcher.fd, client->worker->config->saddr->ai_addr, client->worker->config->saddr->ai_addrlen)) {
		switch (errno) {
			case EINPROGRESS:
			case EALREADY:
				/* async connect now in progress */
				client->state = CLIENT_CONNECTING;
				return 1;
			case EISCONN:
				break;
			case EINTR:
				goto start;
			default:
			{
				strerror_r(errno, client->buffer, sizeof(client->buffer));
				W_ERROR("connect() failed: %s (%d)", client->buffer, errno);
				return 0;
			}
		}
	}

	/* successfully connected */
	client->state = CLIENT_WRITING;     /* 已经连接，则设置client状态为写状态 */
	return 1;
}

/* client的ev_io callback 函数, 其实就是调用client的state machine来处理event事件*/
static void client_io_cb(struct ev_loop *loop, ev_io *w, int revents) {
	Client *client = w->data;   /* ev_io 的data参数，初始化的时候已经赋值为这个client了，有点OO的意思 */

	UNUSED(loop);   /* 这里不知何意 ？？难道就是为了告诉我们没有用到loop和上报的revents */
	UNUSED(revents);

	client_state_machine(client);   /* 实际的事件处理 */
}

void client_state_machine(Client *client) {
	int r;
	Config *config = client->worker->config;

	start:      /* 状态机的开始 */
	//printf("state: %d\n", client->state);
	switch (client->state) {    /* 当前的状态 */
		case CLIENT_START:
			client->worker->stats.req_started++;

			do {
				r = socket(config->saddr->ai_family, config->saddr->ai_socktype, config->saddr->ai_protocol);
			} while (-1 == r && errno == EINTR);

			if (-1 == r) {
				client->state = CLIENT_ERROR;
				strerror_r(errno, client->buffer, sizeof(client->buffer));
				W_ERROR("socket() failed: %s (%d)", client->buffer, errno);
				goto start;
			}

			/* set non-blocking 设置socket句柄nonblock*/
			fcntl(r, F_SETFL, O_NONBLOCK | O_RDWR);

            /* 初始化ev_io，设置callback函数*/
			ev_init(&client->sock_watcher, client_io_cb);
			ev_io_set(&client->sock_watcher, r, EV_WRITE);  /* 设置ev_io，监听写事件*/
			ev_io_start(client->worker->loop, &client->sock_watcher);   /*将ev_io加入到worker的ev_loop中，开启监听*/

            /* 必须在发起connect钱设置fd的写监听事件*/
			if (!client_connect(client)) {/* 如果connect失败，则设置client error*/
				client->state = CLIENT_ERROR;
				goto start;
			} else {
				client_set_events(client, EV_WRITE);  /* 成功，调用client设置可写,
				                                        这里不直接调用ev_io_set()时因为需要先stop再set */
				return;     /* 处理完成，可以返回了,后续会有可写事件再次触发*/
			}
		case CLIENT_CONNECTING: /* 如果是connecting状态，表示已经发出connect，但是并未连上*/
			if (!client_connect(client)) {  /* 再次connect, 有必要吗？？ */
				client->state = CLIENT_ERROR;
				goto start;
			}   /* 这里需要注意，如果连接成功，就直接走到下面wrting状态里去了*/
		case CLIENT_WRITING:
			while (1) { /* dead loop */
			    /* 直接调用write(fd,request[offset],request_size-offset), 将能发送的数据全发送出去 */
				r = write(client->sock_watcher.fd, &config->request[client->request_offset], config->request_size - client->request_offset);
				//printf("write(%d - %d = %d): %d\n", config->request_size, client->request_offset, config->request_size - client->request_offset, r);
				if (r == -1) {  /* 返回 -1， 需要检查err code */
					/* error */
					if (errno == EINTR) /* intr 错误，再继续发送一次 */
						continue;
						                /* 否则，就是发送失败， 设置客户端错误*/
					strerror_r(errno, client->buffer, sizeof(client->buffer));
					W_ERROR("write() failed: %s (%d)", client->buffer, errno);
					client->state = CLIENT_ERROR;
					goto start;
				} else if (r != 0) {    /* >0 表示发送出去数据了，正确 */
					/* success */
					client->request_offset += r;    /* offset 后移动 */
					if (client->request_offset == config->request_size) { /* 如果offset = request_size 表示发送完*/
						/* whole request was sent, start reading */
						client->state = CLIENT_READING;     /* 发送完后，把客户端置为读状态*/
						client_set_events(client, EV_READ); /* ev_io set to 可读事件 */
					}

					return; /* 告一段落，返回*/
				} else {    /* 如果返回0，表示peer reset, 连接断掉了*/
					/* disconnect */
					client->state = CLIENT_END;
					goto start; /* 设置end 返回 */
				}
			}
		case CLIENT_READING:    /* 如果是读状态 */
			while (1) {
			    /* 直接read(fd, buf[offset],buffer_len-offset) */
				r = read(client->sock_watcher.fd, &client->buffer[client->buffer_offset], sizeof(client->buffer) - client->buffer_offset - 1);
				//printf("read(): %d, offset was: %d\n", r, client->buffer_offset);
				if (r == -1) {  /* 返回 -1 */
					/* error */
					if (errno == EINTR) /* intr error, retry */
						continue;
					/* 其它情况，表示错误，直接设置错误code, 返回*/
					strerror_r(errno, client->buffer, sizeof(client->buffer));
					W_ERROR("read() failed: %s (%d)", client->buffer, errno);
					client->state = CLIENT_ERROR;
				} else if (r != 0) {    /* 读到数据了*/
					/* success */
					client->bytes_received += r;
					client->buffer_offset += r;
					client->worker->stats.bytes_total += r;

                    /* 如果overflow了，就之错误 */
					if (client->buffer_offset >= sizeof(client->buffer)) {
						/* too big response header */
						client->state = CLIENT_ERROR;
						break;
					}
					client->buffer[client->buffer_offset] = '\0';
					//printf("buffer:\n==========\n%s\n==========\n", client->buffer);
					if (!client_parse(client, r)) { /* 启动解析 */
						client->state = CLIENT_ERROR;
						//printf("parser failed\n");
						break;
					} else {
						if (client->state == CLIENT_END)
							goto start;
						else
							return;
					}
				} else {    /* ==0, 表示断开 */
					/* disconnect */
					if (client->parser_state == PARSER_BODY && !client->keepalive && client->status_success
						&& !client->chunked && client->content_length == -1) {
						client->success = 1;
						client->state = CLIENT_END;
					} else {
						client->state = CLIENT_ERROR;
					}

					goto start;
				}
			}

		case CLIENT_ERROR:
			//printf("client error\n");
			client->worker->stats.req_error++;
			client->keepalive = 0;
			client->success = 0;
			client->state = CLIENT_END;
		case CLIENT_END:
			/* update worker stats */
			client->worker->stats.req_done++;

			if (client->success) {
				client->worker->stats.req_success++;
				client->worker->stats.bytes_body += client->bytes_received - client->header_size;
			} else {
				client->worker->stats.req_failed++;
			}

			/* print progress every 10% done */
			if (client->worker->id == 1 && client->worker->stats.req_done % client->worker->progress_interval == 0) {
				printf("progress: %3d%% done\n",
					(int) (client->worker->stats.req_done * 100 / client->worker->stats.req_todo)
				);
			}

			if (client->worker->stats.req_started == client->worker->stats.req_todo) {
				/* this worker has started all requests */
				client->keepalive = 0;
				client_reset(client);

				if (client->worker->stats.req_done == client->worker->stats.req_todo) {
					/* this worker has finished all requests */
					ev_unref(client->worker->loop);
				}
			} else {
				client_reset(client);
				goto start;
			}
	}
}


static uint8_t client_parse(Client *client, int size) {
	char *end, *str;
	uint16_t status_code;

	switch (client->parser_state) {
		case PARSER_START:
			//printf("parse (START):\n%s\n", &client->buffer[client->parser_offset]);
			/* look for HTTP/1.1 200 OK */
			if (client->buffer_offset < sizeof("HTTP/1.1 200\r\n"))
				return 1;

			if (strncmp(client->buffer, "HTTP/1.1 ", sizeof("HTTP/1.1 ")-1) != 0)
				return 0;

			// now the status code
			status_code = 0;
			str = client->buffer + sizeof("HTTP/1.1 ")-1;
			for (end = str + 3; str != end; str++) {
				if (*str < '0' || *str > '9')
					return 0;

				status_code *= 10;
				status_code += *str - '0';
			}

			if (status_code >= 200 && status_code < 300) {
				client->worker->stats.req_2xx++;
				client->status_success = 1;
			} else if (status_code < 400) {
				client->worker->stats.req_3xx++;
				client->status_success = 1;
			} else if (status_code < 500) {
				client->worker->stats.req_4xx++;
			} else if (status_code < 600) {
				client->worker->stats.req_5xx++;
			} else {
				// invalid status code
				return 0;
			}

			// look for next \r\n
			end = strchr(end, '\r');
			if (!end || *(end+1) != '\n')
				return 0;

			client->parser_offset = end + 2 - client->buffer;
			client->parser_state = PARSER_HEADER;
		case PARSER_HEADER:
			//printf("parse (HEADER)\n");
			/* look for Content-Length and Connection header */
			while (NULL != (end = strchr(&client->buffer[client->parser_offset], '\r'))) {
				if (*(end+1) != '\n')
					return 0;

				if (end == &client->buffer[client->parser_offset]) {
					/* body reached */
					client->parser_state = PARSER_BODY;
					client->header_size = end + 2 - client->buffer;
					//printf("body reached\n");

					return client_parse(client, size - client->header_size);
				}

				*end = '\0';
				str = &client->buffer[client->parser_offset];
				//printf("checking header: '%s'\n", str);

				if (strncasecmp(str, "Content-Length: ", sizeof("Content-Length: ")-1) == 0) {
					/* content length header */
					client->content_length = str_to_uint64(str + sizeof("Content-Length: ") - 1);
				} else if (strncasecmp(str, "Connection: ", sizeof("Connection: ")-1) == 0) {
					/* connection header */
					str += sizeof("Connection: ") - 1;

					if (strncasecmp(str, "close", sizeof("close")-1) == 0)
						client->keepalive = 0;
					else if (strncasecmp(str, "keep-alive", sizeof("keep-alive")-1) == 0)
						client->keepalive = client->worker->config->keep_alive;
					else
						return 0;
				} else if (strncasecmp(str, "Transfer-Encoding: ", sizeof("Transfer-Encoding: ")-1) == 0) {
					/* transfer encoding header */
					str += sizeof("Transfer-Encoding: ") - 1;

					if (strncasecmp(str, "chunked", sizeof("chunked")-1) == 0)
						client->chunked = 1;
					else
						return 0;
				}


				if (*(end+2) == '\r' && *(end+3) == '\n') {
					/* body reached */
					client->parser_state = PARSER_BODY;
					client->header_size = end + 4 - client->buffer;
					client->parser_offset = client->header_size;
					//printf("body reached\n");

					return client_parse(client, size - client->header_size);
				}

				client->parser_offset = end - client->buffer + 2;
			}

			return 1;
		case PARSER_BODY:
			//printf("parse (BODY)\n");
			/* do nothing, just consume the data */
			/*printf("content-l: %"PRIu64", header: %d, recevied: %"PRIu64"\n",
			client->content_length, client->header_size, client->bytes_received);*/

			if (client->chunked) {
				int consume_max;

				str = &client->buffer[client->parser_offset];
				/*printf("parsing chunk: '%s'\n(%"PRIi64" received, %"PRIi64" size, %d parser offset)\n",
					str, client->chunk_received, client->chunk_size, client->parser_offset
				);*/

				if (client->chunk_size == -1) {
					/* read chunk size */
					client->chunk_size = 0;
					client->chunk_received = 0;
					end = str + size;

					for (; str < end; str++) {
						if (*str == ';' || *str == '\r')
							break;

						client->chunk_size *= 16;
						if (*str >= '0' && *str <= '9')
							client->chunk_size += *str - '0';
						else if (*str >= 'A' && *str <= 'Z')
							client->chunk_size += 10 + *str - 'A';
						else if (*str >= 'a' && *str <= 'z')
							client->chunk_size += 10 + *str - 'a';
						else
							return 0;
					}

					str = strstr(str, "\r\n");
					if (!str)
						return 0;
					str += 2;

					//printf("---------- chunk size: %"PRIi64", %d read, %d offset, data: '%s'\n", client->chunk_size, size, client->parser_offset, str);

					if (client->chunk_size == 0) {
						/* chunk of size 0 marks end of content body */
						client->state = CLIENT_END;
						client->success = client->status_success ? 1 : 0;
						return 1;
					}

					size -= str - &client->buffer[client->parser_offset];
					client->parser_offset = str - client->buffer;
				}

				/* consume chunk till chunk_size is reached */
				consume_max = client->chunk_size - client->chunk_received;

				if (size < consume_max)
					consume_max = size;

				client->chunk_received += consume_max;
				client->parser_offset += consume_max;

				//printf("---------- chunk consuming: %d, received: %"PRIi64" of %"PRIi64", offset: %d\n", consume_max, client->chunk_received, client->chunk_size, client->parser_offset);

				if (client->chunk_received == client->chunk_size) {
					if (client->buffer[client->parser_offset] != '\r' || client->buffer[client->parser_offset+1] != '\n')
						return 0;

					/* got whole chunk, next! */
					//printf("---------- got whole chunk!!\n");
					client->chunk_size = -1;
					client->chunk_received = 0;
					client->parser_offset += 2;
					consume_max += 2;

					/* there is stuff left to parse */
					if (size - consume_max > 0)
						return client_parse(client, size - consume_max);
				}

				client->parser_offset = 0;
				client->buffer_offset = 0;

				return 1;
			} else {
				/* not chunked, just consume all data till content-length is reached */
				client->buffer_offset = 0;

				if (client->content_length == -1)
					return 0;

				if (client->bytes_received == (uint64_t) (client->header_size + client->content_length)) {
					/* full response received */
					client->state = CLIENT_END;
					client->success = client->status_success ? 1 : 0;
				}
			}

			return 1;
	}

	return 1;
}
