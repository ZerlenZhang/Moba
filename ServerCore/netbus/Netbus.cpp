#include "Netbus.h"
#include <uv.h>
#include "../../netbus/protocol/WebSocketProtocol.h"
#include "../../netbus/protocol/TcpPackageProtocol.h"
#include "../../netbus/session/TcpSession.h"

#include "service/ServiceManager.h"

#include "../utils/logger/logger.h"
#include "session/UdpSession.h"

#include "../utils/cache_alloc/small_alloc.h"

#define my_alloc small_alloc
#define my_free small_free

#pragma region 函数声明

void OnRecvCommond(AbstractSession* session, unsigned char* body, const int len);

void OnWebSocketRecvData(TcpSession* session);

void OnTcpRecvData(TcpSession* session);

#pragma endregion

struct UdpRecvBuf
{
	char* data;
	int maxRecvLen;
};

struct TcpConnectInfo
{
	TcpConnectedCallback cb;
	void* data;
};

struct TcpListenInfo
{
	TcpListenCallback cb;
	SocketType socketType;
	void* data;
};

#pragma region 回调函数

extern "C"
{
#pragma region Tcp_&_Websocket

	//Tcp申请字符串空间
	static void tcp_str_alloc(uv_handle_t* handle,
		size_t suggested_size,
		uv_buf_t* buf) {

		auto session = (TcpSession*)handle->data;

		if (session->recved < RECV_LEN)
		{
			//session的长度为 RECV_LEN 的recvBuf还没有存满
			*buf = uv_buf_init(session->recvBuf + session->recved, RECV_LEN - session->recved);
		}
		else
		{// recvBuf读满了，但是还没有读完 


			if (session->long_pkg == NULL)
			{// 如果还没有new空间
				int pkgSize;
				int headSize;
				switch (session->socketType)
				{
				case SocketType::TcpSocket:
					TcpProtocol::ReadHeader((unsigned char*)session->recvBuf, session->recved, &pkgSize, &headSize);
					break;
				case SocketType::WebSocket:
					WebSocketProtocol::ReadHeader((unsigned char*)session->recvBuf, session->recved, &pkgSize, &headSize);
					break;
				}
				session->long_pkg_size = pkgSize;
				session->long_pkg = (char*)malloc(pkgSize);

				memcpy(session->long_pkg, session->recvBuf, session->recved);
			}

			*buf = uv_buf_init(session->long_pkg + session->recved, session->long_pkg_size - session->recved);
		}

	}

	//读取结束后的回调
	static void tcp_after_read(uv_stream_t* stream,
		ssize_t nread,
		const uv_buf_t* buf) {

		auto session = (TcpSession*)stream->data;

		//连接断开
		if (nread < 0) {
			session->Close();
			return;
		}

		session->recved += nread;

		switch (session->socketType)
		{
		case SocketType::TcpSocket:
			OnTcpRecvData(session);
			break;
		case SocketType::WebSocket:
			#pragma region WebSocket协议	
			if (session->isWebSocketShakeHand == 0)
			{	//	shakeHand
				if (WebSocketProtocol::ShakeHand(session, session->recvBuf, session->recved))
				{	//握手成功
					log_debug("握手成功");
					session->isWebSocketShakeHand = 1;
					session->recved = 0;
				}
			}
			else//	recv/send Data
			{
				OnWebSocketRecvData(session);
			}
			#pragma endregion
			break;

		default:
			break;
		}
	}

	//TCP有用户链接进来
	static void TcpOnConnect(uv_stream_t* server, int status)
	{
		auto info = (TcpListenInfo*)server->data;
		auto session = TcpSession::Create();

		//初始化session的信息——socket类型，ip，端口，并加入libuv事件循环
		session->Init(info->socketType);
		
		//客户端接入服务器 
		uv_accept(server, (uv_stream_t*)&session->tcpHandle);

		//回调
		if(info->cb)
			info->cb(session, info->data);

		//Session连接成功的回调
		ServiceManager::OnSessionConnect(session);

		//开始监听消息
		uv_read_start((uv_stream_t*)&session->tcpHandle, tcp_str_alloc, tcp_after_read);
		
	}

#pragma endregion

#pragma region Udp

	//Udp申请字符串空间
	static void udp_str_alloc(uv_handle_t* handle,
		size_t suggested_size,
		uv_buf_t* buf)
	{
		suggested_size = (suggested_size < 4096) ? 4096 : suggested_size;

		auto pBuf = (UdpRecvBuf*)handle->data;
		if (pBuf->maxRecvLen < suggested_size)
		{// 表明当前空间不够
			if (pBuf->data)
			{
				free(pBuf->data);
				pBuf->data = NULL;
			}
			pBuf->data = (char*)malloc(suggested_size);
			pBuf->maxRecvLen = suggested_size;

		}

		buf->base = pBuf->data;
		buf->len = suggested_size;
	}

	//Udp接收字符串完成
	static void udp_after_recv(uv_udp_t* handle,
		ssize_t nread,
		const uv_buf_t* buf,
		const struct sockaddr* addr,
		unsigned flags)
	{
		if (nread <= 0)
			return;
		UdpSession us;

		us.Init(handle, addr);

		OnRecvCommond(&us, (unsigned char*)buf->base, nread);
	}

#pragma endregion

	void AfterUdpSend(uv_udp_send_t* req, int status)
	{
		if (status)
		{
			log_error("udp发送失败");
		}
		my_free(req);
	}
}

#pragma endregion


static void OnRecvCommond(AbstractSession* session, unsigned char* body, const int bodyLen)
{
	RawPackage raw;
	if (CmdPackageProtocol::DecodeBytesToRawPackage(body, bodyLen, &raw))
	{
		//调用服务管理器处理各种命令
		if (!ServiceManager::OnRecvRawPackage(session, &raw))
		{
			session->Close();
		}		
	}
	else
	{
		int port;
		auto ip = session->GetAddress(port);
		log_error("[%s:%d]发来的包解码失败",ip,port);
	}
}


#pragma region WebSocketProtocol


static void OnWebSocketRecvData(TcpSession* session)
{
	auto pkgData = (unsigned char*)(session->long_pkg != NULL ? session->long_pkg : session->recvBuf);

	while (session->recved > 0)
	{
		//是否是关闭包
		if (pkgData[0] == 0x88) {
			log_debug("收到关闭包");
			session->Close();
			return;
		}

		int pkgSize;
		int headSize;
		if (!WebSocketProtocol::ReadHeader(pkgData, session->recved, &pkgSize, &headSize))
		{// 读取包头失败
			log_debug("读取包头失败");
			break;
		}

		if (session->recved < pkgSize)
		{// 没有收完数据
			log_debug("没有收完数据");
			break;
		}

		//掩码位置紧随头部数据之后
		//body位置在掩码之后
		unsigned char* body = pkgData + headSize;
		unsigned char* mask = body - 4;

		//解析收到的纯数据
		WebSocketProtocol::ParserRecvData(body, mask, pkgSize - headSize);

		//处理收到的完整数据包
		OnRecvCommond(session, body, pkgSize);

		if (session->recved > pkgSize)
		{
			memmove(pkgData, pkgData + pkgSize, session->recved - pkgSize);
		}

		//每次减去读取到的长度
		session->recved -= pkgSize;

		//如果长包处理完了
		if (session->recved == 0 && session->long_pkg != NULL)
		{
			free(session->long_pkg);
			session->long_pkg = NULL;
			session->long_pkg_size = 0;
		}
	}
}

#pragma endregion

#pragma region TcpProtocol

static void OnTcpRecvData(TcpSession* session)
{
	auto tcpPkgData = (unsigned char*)(session->long_pkg != NULL ? session->long_pkg : session->recvBuf);

	while (session->recved > 0)
	{
		int tcpPkgSize;
		int tcpHeadSize;
		if (!TcpProtocol::ReadHeader(tcpPkgData, session->recved, &tcpPkgSize, &tcpHeadSize))
		{// 读取包头失败
			int port;
			auto ip = session->GetAddress(port);
			log_error("TCP读取包头失败,包体来自：[%s:%d]",ip,port);
			break;
		}

		if (tcpPkgSize < tcpHeadSize) {
			session->Close();
			int port;
			auto ip = session->GetAddress(port);
			log_warning("收到[%s:%d]发来的非法包【包体大小小于包头】，session已关闭",ip,port);
			break;
		}

		if (session->recved < tcpPkgSize)
		{// 没有收完数据
			log_debug("没有收完数据");
			break;
		}

		//body位置在掩码之后
		unsigned char* body = tcpPkgData + tcpHeadSize;

		//处理收到的完整数据包
		OnRecvCommond(session, body, tcpPkgSize - tcpHeadSize);

		if (session->recved > tcpPkgSize)
		{
			memmove(tcpPkgData, tcpPkgData + tcpPkgSize, session->recved - tcpPkgSize);
		}

		//每次减去读取到的长度
		session->recved -= tcpPkgSize;

		//如果长包处理完了
		if (session->recved == 0 && session->long_pkg != NULL)
		{
			free(session->long_pkg);
			session->long_pkg = NULL;
			session->long_pkg_size = 0;
		}
	}
}


#pragma endregion

static void after_connect(uv_connect_t* handle, int status)
{
	auto session = (AbstractSession*)handle->handle->data;
	auto info = (TcpConnectInfo*)handle->data;

	//如果有异常情况
	if (status) 
	{
		if (info)
		{
			if(info->cb)
				info->cb(1, NULL, info->data);
			my_free(info);
			info = NULL;
		}
		if(session)
			session->Close();
		my_free(handle);
		return;
	}

	//如果一切正常
	if (info)
	{
		if (info->cb)
		{
			info->cb(0, session, info->data);
		}
		my_free(info);
		info = NULL;
	}
	uv_read_start(handle->handle, tcp_str_alloc, tcp_after_read);
	my_free(handle);
}



static Netbus g_instance;
Netbus* Netbus::Instance()
{
	return &g_instance;
}

Netbus::Netbus()
{
	this->udpHandle = NULL;
}

void Netbus::TcpListen(int port, TcpListenCallback callback, void* udata)const
{
	auto listen = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));

	memset(listen, 0, sizeof(uv_tcp_t));

	sockaddr_in addr;
	auto ret = uv_ip4_addr("0.0.0.0", port, &addr);
	if (ret)
	{
		log_error("uv_ip4_addr error：%d", port);
		free(listen);
		listen = NULL;
		return;

	}

	uv_tcp_init(uv_default_loop(), listen);
	ret = uv_tcp_bind(listen, (const sockaddr*)&addr, 0);
	if (ret)
	{
		log_error("Tcp 端口绑定失败：%d",port);
		free(listen);
		listen = NULL;
		return;
	}

	
	auto pInfo = new TcpListenInfo();
	pInfo->cb = callback;
	pInfo->data = udata;
	pInfo->socketType = SocketType::TcpSocket;

	//强转记录TcpListenInfo类型
	listen->data = (void*)pInfo;

	uv_listen((uv_stream_t*)listen, SOMAXCONN, TcpOnConnect);
}

void Netbus::WebSocketListen(int port, TcpListenCallback callback, void* udata)const
{
	auto listen = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));

	memset(listen, 0, sizeof(uv_tcp_t));

	sockaddr_in addr;
	auto ret = uv_ip4_addr("0.0.0.0", port, &addr);
	if (ret)
	{
		log_error("uv_ip4_addr error：%d", port);
		free(listen);
		listen = NULL;
		return;

	}

	uv_tcp_init(uv_default_loop(), listen);
	ret = uv_tcp_bind(listen, (const sockaddr*)&addr, 0);
	if (0 != ret)
	{
		log_error("WebSocket 端口绑定失败：%d",port);
		free(listen);
		listen = NULL;
		return;
	}


	auto pInfo = new TcpListenInfo();
	pInfo->cb = callback;
	pInfo->data = udata;
	pInfo->socketType = SocketType::WebSocket;

	//强转记录TcpListenInfo类型
	listen->data = (void*)pInfo;

	uv_listen((uv_stream_t*)listen, SOMAXCONN, TcpOnConnect);
}

void Netbus::UdpListen(int port)
{
	if (this->udpHandle)
	{
		log_warning("暂时不支持同时监听多个udp");
		return;
	}

	sockaddr_in addr;
	auto ret = uv_ip4_addr("0.0.0.0", port, &addr);
	if (ret)
	{
		log_error("uv_ip4_addr error：%d", port);
		return;
	}
	auto server = (uv_udp_t*)malloc(sizeof(uv_udp_t));
	memset(server, 0, sizeof(uv_udp_t));

	uv_udp_init(uv_default_loop(), server);

	server->data = malloc(sizeof(UdpRecvBuf));
	memset(server->data, 0, sizeof(UdpRecvBuf));
	ret = uv_udp_bind(server, (const sockaddr*)&addr, 0);

	if (ret)
	{
		log_error("Udp 绑定失败");
		if (server)
		{
			if (server->data)
				free(server->data);
			free(server);
			server = NULL;
			return;
		}
	}

	this->udpHandle = server;

	uv_udp_recv_start(server, udp_str_alloc, udp_after_recv);
}

void Netbus::Run()const
{
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}

void Netbus::Init() const
{
	ServiceManager::Init();
	InitAllocers();
}

void Netbus::TcpConnect(const char* serverIp, int port, TcpConnectedCallback callback, void* udata)const
{
	sockaddr_in addr;
	auto ret = uv_ip4_addr(serverIp, port, &addr);
	if (ret)
	{
		log_error("uv_ip4_addr error：%s:%d", serverIp,port);
		return;

	}

	//创建Session并初始化相关值——socketType,ip,port,isCliet，并加入libuv事件循环
	auto s = TcpSession::Create();
	s->Init(SocketType::TcpSocket, serverIp, port, true);

	auto req = (uv_connect_t*)my_alloc(sizeof(uv_connect_t));
	memset(req, 0, sizeof(uv_connect_t));

	auto info = (TcpConnectInfo*)my_alloc(sizeof(TcpConnectInfo));
	memset(info, 0, sizeof(TcpConnectInfo));

	info->cb = callback;
	info->data = udata;
	req->data = info;

	ret = uv_tcp_connect(req, &s->tcpHandle, (struct sockaddr*) & addr, after_connect);
}

void Netbus::UdpSendTo(char* ip, int port, unsigned char* body, int len)
{
	auto wbuf = uv_buf_init((char*)body, len);
	auto req = (uv_udp_send_t*)my_alloc(sizeof(uv_udp_send_t));

	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.S_un.S_addr = inet_addr(ip);

	uv_udp_send(req, (uv_udp_t*)this->udpHandle, &wbuf, 1,(const SOCKADDR*) &addr, AfterUdpSend);
}
 