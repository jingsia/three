/**
 * login.cpp
 *
 * 登录注册相关的协议处理，可以1个文件放一个协议处理，也可以多个
 */

#include "protocol_define.h"
#include "cmd_factory.h"

#include "dnl.pb.h"
#include "s2s_dnl.pb.h"

#include "../manager_server.h"
#include "server_list.h"
#include "sql_helper.h"
#include "net_peer_mgr.h"
#include "Tools.h"

#ifdef WIN32
#include <objbase.h>
#else
#include <uuid/uuid.h>
#endif
#include <stdio.h>

#include <MD5.h>
#include <convert.h>

using namespace std;

typedef struct _GUID
{
    unsigned long Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char Data4[8];
} GUID, UUID;

GUID CreateGuid()
{
	GUID guid;
#ifdef WIN32
	CoCreateGuid(&guid);
#else
	uuid_generate(reinterpret_cast<unsigned char *>(&guid));
#endif
	return guid;
}

std::string GuidToString(const GUID &guid)
{
	char buf[64] = {0};
#ifdef __GNUC__
	snprintf(
#else
    _snprintf_s(
#endif
			buf,
			sizeof(buf),
			"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
			guid.Data1, guid.Data2, guid.Data3,
			guid.Data4[0], guid.Data4[1],
			guid.Data4[2], guid.Data4[3],
			guid.Data4[4], guid.Data4[5],
			guid.Data4[6], guid.Data4[7]);

		return std::string(buf);
}

MSG_HANDLE(E_S2S_REGISTER_SID_INFO)
{
	REQUEST(E_S2S_REGISTER_SID_INFO)  recv;
	REQUEST(E_S2S_REGISTER_SID_INFO)  sendLGS;

	if(recv.ParseFromArray(smsg->GetBodyBuf(),smsg->GetBodySize()))
	{
		LDEBUG(g_PBPrinter.getJsonStream(recv));

		if(smsg->GetSecHead().servertype == SERVER_GG)
		{
			string send_server_msg;
			sendLGS.set_name(recv.name());
			sendLGS.set_pwd(recv.pwd());
			//sendLGS.set_id(recv.id());
			sendLGS.set_cg_sid(recv.cg_sid());
			sendLGS.set_gg_sid(smsg->GetConnection()->GetSocketFD());
			//sendLGS.set_gs_sid(smsg->GetConnection()->GetSocketFD());

			sendLGS.SerializeToString(&send_server_msg);

			SMessage ssmsg;
			ssmsg.SetHead(E_S2S_REGISTER_SID_INFO,E_S2S_REGISTER_SID_INFO, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
			ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

			g_ManagerServer.getGlobalServer()->writeMsg((BMessage*)&ssmsg);
		}
		else if(smsg->GetSecHead().servertype == SERVER_LGS)
		{
			SMessage ssmsg;
			ssmsg.SetHead(smsg->GetFstHead().msgtype,smsg->GetFstHead().msgtype, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
			ssmsg.SetContent((uint8_t*)smsg->GetBodyBuf(),smsg->GetBodySize());

			Session * psession = (Session *)g_ConnectionMgt.GetSocket(recv.gg_sid());
			if(NULL == psession)
			{
				LWARN("Message ssid " <<recv.gs_sid()<< " not exist");
			}
			else
			{
				psession->writeMsg((BMessage*)&ssmsg);
			}
		}
	}
	else
	{
		cout << "E_S2S_REGISTER_SID_INFO parse failed" << endl;
	}

	return OPER_RES_DONNOT_RETURN;
}

MSG_HANDLE(E_S2S_LOGIN_SID_INFO)
{
	REQUEST(E_S2S_LOGIN_SID_INFO)  recv;
	REQUEST(E_S2S_LOGIN_SID_INFO)  sendLGS;

	if(recv.ParseFromArray(smsg->GetBodyBuf(),smsg->GetBodySize()))
	{
		LDEBUG(g_PBPrinter.getJsonStream(recv));

		if(smsg->GetSecHead().servertype == SERVER_GG)
		{
			//区分进入不同平台
			if(recv.sdk_key() == "YX")
			{
				//转发LGS
				string send_server_msg;
				sendLGS.set_name(recv.name());
				sendLGS.set_pwd(recv.pwd());
				sendLGS.set_cg_sid(recv.cg_sid());
				sendLGS.set_gg_sid(smsg->GetConnection()->GetSocketFD());
				sendLGS.set_sdk_key("YX");

				sendLGS.SerializeToString(&send_server_msg);

				SMessage ssmsg;
				ssmsg.SetHead(E_S2S_LOGIN_SID_INFO,E_S2S_LOGIN_SID_INFO, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
				ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

				g_ManagerServer.getGlobalServer()->writeMsg((BMessage*)&ssmsg);
			}
			else if(recv.sdk_key() == "SD" || recv.sdk_key() == "GH")
			{
				//http请求登录信息
				time_t timep;
				time(&timep);

				GUID guid_;
				guid_ = CreateGuid();
				string sequence = GuidToString(guid_);

				//791000159 3e12c18085b0e6ec358ea27899214187
				string appid = number_to_str(791000164);
				string ticket_id = recv.pwd();
				string timestamp = number_to_str(timep);
				string sdk = "api.mygm.sdo.com/v1/open/ticket?appid=" + appid + "&sequence=" + sequence + "&ticket_id=" + ticket_id + "&timestamp=" + timestamp;
				string md5_str = "appid=" + appid + "&sequence=" + sequence + "&ticket_id=" + ticket_id + "&timestamp=" + timestamp + "6e6770e716669f46b7752c7c50a213b9";
				LDEBUG("md5_str:"<<md5_str);
				string sign = frame::md5(md5_str);
				LDEBUG("sdk sign:"<<sign);
				sdk = sdk + "&sign=" + sign;
				//char c[sdk.length()];
				//strcpy(c,sdk.c_str());
				LDEBUG("sdk url:"<<sdk);


				//把http请求发送给 http_server 等处理结果再返回
				Json::Value out;
				out["url"] = sdk;
				out["method"] = "GET";	// url和 method参数必须有
				//out["method"] = "POST";	//如果是POST，必须有postdata
				//out["postdata"] = data+"&verifystring="+verifystring;
				//out["headers"].append(Json::Value("charset=UTF-8")); //头信息，可选，可以多个

				//到时候需要返回的参数，可在这里传，返回时会透传
				out["msgid"] = E_S2S_LOGIN_SID_INFO;	//当时哪个协议发过来
				out["name"] = recv.name();
				out["pwd"] = recv.pwd();
				out["sdk_key"] = recv.sdk_key();
				out["cg_sid"] = recv.cg_sid();
				out["gg_sid"] = smsg->GetConnection()->GetSocketFD();

				string send_server_msg = out.toStyledString();
				SMessage ssmsg;
				ssmsg.SetHead(E_S2S_HTTP_REQUEST,E_S2S_HTTP_REQUEST, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
				ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

				g_ManagerServer.getHttpServer()->writeMsg((BMessage*)&ssmsg);

				return OPER_RES_DONNOT_RETURN;

			}
			else if(recv.sdk_key() == "KY")
			{
				string tokenkey = recv.pwd();
				string appkey = "847bb44339805cf33b21f157c986fcd0";	//龙之佣兵 的 appkey
				string md5_str =  appkey  + tokenkey;
				LDEBUG("md5_str:"<<md5_str);
				string sign = frame::md5(md5_str);
				LDEBUG("ky sdk sign:"<<sign);
				string url = "http://f_signin.bppstore.com/loginCheck.php?tokenKey="+tokenkey+"&sign="+sign;

				LDEBUG("ky sdk url:"<<url);

				//把http请求发送给 http_server 等处理结果再返回
				Json::Value out;
				out["url"] = url;
				out["method"] = "GET";	// url和 method参数必须有

				//到时候需要返回的参数，可在这里传，返回时会透传
				out["msgid"] = E_S2S_LOGIN_SID_INFO;	//当时哪个协议发过来
				out["name"] = recv.name();
				out["pwd"] = recv.pwd();
				out["sdk_key"] = recv.sdk_key();
				out["cg_sid"] = recv.cg_sid();
				out["gg_sid"] = smsg->GetConnection()->GetSocketFD();

				string send_server_msg = out.toStyledString();
				SMessage ssmsg;
				ssmsg.SetHead(E_S2S_HTTP_REQUEST,E_S2S_HTTP_REQUEST, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
				ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

				g_ManagerServer.getHttpServer()->writeMsg((BMessage*)&ssmsg);

				return OPER_RES_DONNOT_RETURN;

			}
			else if(recv.sdk_key() == "XY")
			{
				string tokenkey = recv.pwd();

				string url = "http://passport.xyzs.com/checkLogin.php";

				LDEBUG("xy sdk url:"<<url);

				//把http请求发送给 http_server 等处理结果再返回
				Json::Value out;
				out["url"] = url;
				out["method"] = "POST";	//如果是POST，必须有postdata
				//appid = 100025392;100027333
				out["postdata"] = "appid=100025392&token="+tokenkey+"&uid="+recv.name();
				out["headers"].append(Json::Value("charset=UTF-8")); //头信息，可选，可以多个

				//到时候需要返回的参数，可在这里传，返回时会透传
				out["msgid"] = E_S2S_LOGIN_SID_INFO;	//当时哪个协议发过来
				out["name"] = recv.name();
				out["pwd"] = recv.pwd();
				out["sdk_key"] = recv.sdk_key();
				out["cg_sid"] = recv.cg_sid();
				out["gg_sid"] = smsg->GetConnection()->GetSocketFD();

				string send_server_msg = out.toStyledString();
				SMessage ssmsg;
				ssmsg.SetHead(E_S2S_HTTP_REQUEST,E_S2S_HTTP_REQUEST, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
				ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

				g_ManagerServer.getHttpServer()->writeMsg((BMessage*)&ssmsg);

				return OPER_RES_DONNOT_RETURN;

			}
			else if(recv.sdk_key() == "TB")
			{
				string tokenkey = recv.pwd();
				string appid = "151159";
				string url = "http://tgi.tongbu.com/api/LoginCheck.ashx?session="+tokenkey+"&appid="+appid;

				LDEBUG("TB sdk url:"<<url);

				//把http请求发送给 http_server 等处理结果再返回
				Json::Value out;
				out["url"] = url;
				out["method"] = "GET";	// url和 method参数必须有

				//到时候需要返回的参数，可在这里传，返回时会透传
				out["msgid"] = E_S2S_LOGIN_SID_INFO;	//当时哪个协议发过来
				out["name"] = recv.name();
				out["pwd"] = recv.pwd();
				out["sdk_key"] = recv.sdk_key();
				out["cg_sid"] = recv.cg_sid();
				out["gg_sid"] = smsg->GetConnection()->GetSocketFD();

				string send_server_msg = out.toStyledString();
				SMessage ssmsg;
				ssmsg.SetHead(E_S2S_HTTP_REQUEST,E_S2S_HTTP_REQUEST, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
				ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

				g_ManagerServer.getHttpServer()->writeMsg((BMessage*)&ssmsg);

				return OPER_RES_DONNOT_RETURN;

			}
			else if(recv.sdk_key() == "HM")
			{
				string tokenkey = recv.pwd();

				string url = "http://api.haimawan.com/index.php?m=api&a=validate_token";

				LDEBUG("hm sdk url:"<<url);

				//把http请求发送给 http_server 等处理结果再返回
				Json::Value out;
				out["url"] = url;
				out["method"] = "POST";	//如果是POST，必须有postdata
				out["postdata"] = "appid=3b32d18d07a8e9b462a9ad339d7c93d2&t="+tokenkey+"&uid="+recv.name();
				out["headers"].append(Json::Value("charset=UTF-8")); //头信息，可选，可以多个

				//到时候需要返回的参数，可在这里传，返回时会透传
				out["msgid"] = E_S2S_LOGIN_SID_INFO;	//当时哪个协议发过来
				out["name"] = recv.name();
				out["pwd"] = recv.pwd();
				out["sdk_key"] = recv.sdk_key();
				out["cg_sid"] = recv.cg_sid();
				out["gg_sid"] = smsg->GetConnection()->GetSocketFD();

				string send_server_msg = out.toStyledString();
				SMessage ssmsg;
				ssmsg.SetHead(E_S2S_HTTP_REQUEST,E_S2S_HTTP_REQUEST, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
				ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

				g_ManagerServer.getHttpServer()->writeMsg((BMessage*)&ssmsg);

				return OPER_RES_DONNOT_RETURN;

			}
			else if(recv.sdk_key() == "AS")
			{
				string token = recv.pwd();
				//string appid = "151159";
				string url = "http://pay.i4.cn/member_third.action?token="+token;

				LDEBUG("AS sdk url:"<<url);

				//把http请求发送给 http_server 等处理结果再返回
				Json::Value out;
				out["url"] = url;
				out["method"] = "GET";	// url和 method参数必须有

				//到时候需要返回的参数，可在这里传，返回时会透传
				out["msgid"] = E_S2S_LOGIN_SID_INFO;	//当时哪个协议发过来
				out["name"] = recv.name();
				out["pwd"] = recv.pwd();
				out["sdk_key"] = recv.sdk_key();
				out["cg_sid"] = recv.cg_sid();
				out["gg_sid"] = smsg->GetConnection()->GetSocketFD();

				string send_server_msg = out.toStyledString();
				SMessage ssmsg;
				ssmsg.SetHead(E_S2S_HTTP_REQUEST,E_S2S_HTTP_REQUEST, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
				ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

				g_ManagerServer.getHttpServer()->writeMsg((BMessage*)&ssmsg);

				return OPER_RES_DONNOT_RETURN;

			}
			else if(recv.sdk_key() == "GP")
			{
				//http请求登录信息
				time_t timep;
				time(&timep);

				string appid = number_to_str(105639);
				string tokenkey = recv.pwd();
				string game_uin = recv.name();
				string timestamp = number_to_str(timep);
				string server_key = "6A0K4M92WQJOQGWQN4CN2CHXK52ZUK1JTFBLX96J98YWH53A4EZJCKQY9AZUVKXE";
				string sdk = "http://userapi.guopan.cn/gamesdk/verify/?game_uin=" + game_uin + "" + "&appid=" + appid + "&token=" + tokenkey + "&t=" + timestamp;
				string md5_str = game_uin + appid + timestamp + server_key;
				LDEBUG("md5_str:"<<md5_str);
				string sign = frame::md5(md5_str);
				LDEBUG("sdk sign:"<<sign);
				sdk = sdk + "&sign=" + sign;

				LDEBUG("sdk url:"<<sdk);

				//把http请求发送给 http_server 等处理结果再返回
				Json::Value out;
				out["url"] = sdk;
				out["method"] = "GET";	// url和 method参数必须有

				//到时候需要返回的参数，可在这里传，返回时会透传
				out["msgid"] = E_S2S_LOGIN_SID_INFO;	//当时哪个协议发过来
				out["name"] = recv.name();
				out["pwd"] = recv.pwd();
				out["sdk_key"] = recv.sdk_key();
				out["cg_sid"] = recv.cg_sid();
				out["gg_sid"] = smsg->GetConnection()->GetSocketFD();

				string send_server_msg = out.toStyledString();
				SMessage ssmsg;
				ssmsg.SetHead(E_S2S_HTTP_REQUEST,E_S2S_HTTP_REQUEST, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
				ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

				g_ManagerServer.getHttpServer()->writeMsg((BMessage*)&ssmsg);

				return OPER_RES_DONNOT_RETURN;

			}
			else if(recv.sdk_key() == "JG")
			{
				//http请求登录信息				
				struct timeval tv;    
				gettimeofday(&tv,NULL);    
				int64_t timep = tv.tv_sec * 1000 + tv.tv_usec / 1000;

				string clientID = number_to_str(1033);
				string ticket = recv.pwd();		
				string timestamp = number_to_str(timep);
				string server_key = "2WimhXXkDxDSQ1W8671y";
				string sdk = "http://api.nutsplay.com/SDK/verify.do?clientID=" + clientID + "&ticket=" + ticket + "&time=" + timestamp;
				string md5_str = clientID + ticket + timestamp + server_key;
				LDEBUG("md5_str:"<<md5_str);
				string sign = frame::md5(md5_str);
				LDEBUG("sdk sign:"<<sign);
				sdk = sdk + "&mac=" + sign;

				LDEBUG("sdk url:"<<sdk);

				//把http请求发送给 http_server 等处理结果再返回
				Json::Value out;
				out["url"] = sdk;
				out["method"] = "GET";	// url和 method参数必须有

				//到时候需要返回的参数，可在这里传，返回时会透传
				out["msgid"] = E_S2S_LOGIN_SID_INFO;	//当时哪个协议发过来
				out["name"] = recv.name();
				out["pwd"] = recv.pwd();
				out["sdk_key"] = recv.sdk_key();
				out["cg_sid"] = recv.cg_sid();
				out["gg_sid"] = smsg->GetConnection()->GetSocketFD();

				string send_server_msg = out.toStyledString();
				SMessage ssmsg;
				ssmsg.SetHead(E_S2S_HTTP_REQUEST,E_S2S_HTTP_REQUEST, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
				ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

				g_ManagerServer.getHttpServer()->writeMsg((BMessage*)&ssmsg);

				return OPER_RES_DONNOT_RETURN;

			}	
			else
			{
				//直接返回GG错误值
				LWARN("LOGIN sdk_key error."<<recv.sdk_key());
				REQUEST(E_S2S_LOGIN_SID_INFO)  resp;
				resp.set_isok(E_ERROR_LOGIN_SDK_ERROR);
				resp.set_cg_sid(recv.cg_sid());
				resp.SerializeToString(&content);

				return OPER_RES_STRING;
			}
		}
		else if(smsg->GetSecHead().servertype == SERVER_LGS)
		{
			if(recv.isok() != E_CORRECT)
			{
				SMessage ssmsg;
				ssmsg.SetHead(smsg->GetFstHead().msgtype,smsg->GetFstHead().msgtype, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
				ssmsg.SetContent((uint8_t*)smsg->GetBodyBuf(),smsg->GetBodySize());
				Session * psession = (Session *)g_ConnectionMgt.GetSocket(recv.gg_sid());
				if(NULL == psession)
				{
					LWARN("Message ssid " <<recv.gg_sid()<< " not exist");
				}
				else
				{
					psession->writeMsg((BMessage*)&ssmsg);
				}
			}
			else
			{
				//发给本地数据服
				REQUEST(E_S2S_LOGIN_SID_INFO)  sendDB;
				string send_server_msg;
				sendDB.set_name(recv.name());
				sendDB.set_pwd(recv.pwd());
				sendDB.set_id(recv.id());
				sendDB.set_cg_sid(recv.cg_sid());
				sendDB.set_gg_sid(recv.gg_sid());

				sendDB.SerializeToString(&send_server_msg);

				SMessage ssmsg;
				ssmsg.SetHead(E_S2S_LOGIN_SID_INFO,E_S2S_LOGIN_SID_INFO, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
				ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

				g_ServerList.broadcastMsg(SERVER_DB,(BMessage*)&ssmsg);
				//g_ManagerServer.getDataServer()->writeMsg((BMessage*)&ssmsg);
			}
		}
		else if(smsg->GetSecHead().servertype == SERVER_DB)
		{
			//回复给网关
		  SMessage ssmsg;
			ssmsg.SetHead(smsg->GetFstHead().msgtype,smsg->GetFstHead().msgtype, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
			ssmsg.SetContent((uint8_t*)smsg->GetBodyBuf(),smsg->GetBodySize());

			Session * psession = (Session *)g_ConnectionMgt.GetSocket(recv.gg_sid());
			if(NULL == psession)
			{
				LWARN("Message ssid " <<recv.gg_sid()<< " not exist");
			}
			else
			{
				psession->writeMsg((BMessage*)&ssmsg);
			}
		}
	}
	else
	{
		cout << "E_S2S_LOGIN_SID_INFO parse failed" << endl;
	}

	return OPER_RES_DONNOT_RETURN;
}

MSG_HANDLE(E_S2S_DB_CHECK_NAME)
{
	REQUEST(E_S2S_DB_CHECK_NAME)  recv;
	REQUEST(E_S2S_DB_CHECK_NAME)  sendLGS;

	if(recv.ParseFromArray(smsg->GetBodyBuf(),smsg->GetBodySize()))
	{
		LDEBUG(g_PBPrinter.getJsonStream(recv));

		if(smsg->GetSecHead().servertype == SERVER_GS)
		{
			string send_server_msg;
			sendLGS.set_name(recv.name());
			sendLGS.set_delayid(recv.delayid());
			sendLGS.set_gs_sid(smsg->GetConnection()->GetSocketFD());

			sendLGS.SerializeToString(&send_server_msg);

			SMessage ssmsg;
			ssmsg.SetHead(E_S2S_DB_CHECK_NAME,E_S2S_DB_CHECK_NAME, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,0);
			ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

			g_ManagerServer.getGlobalServer()->writeMsg((BMessage*)&ssmsg);
		}
		else if(smsg->GetSecHead().servertype == SERVER_LGS)
		{
			if(recv.isok() != E_CORRECT)
			{
				//返回GG
				string resp_msg;
				RESPOND(E_C2S_CREATE_IMAGE) respGG;
				respGG.set_isok(recv.isok());
				respGG.SerializeToString(&resp_msg);

				SMessage ssmsg;
				ssmsg.SetHead(E_C2S_CREATE_IMAGE,smsg->GetFstHead().msgtype, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
				ssmsg.SetContent((uint8_t*)resp_msg.c_str(),resp_msg.length());

				Session * psession = (Session *)g_ConnectionMgt.GetSocket(recv.gg_sid());
				if(NULL == psession)
				{
					LWARN("Message ssid " <<recv.gg_sid()<< " not exist");
				}
				else
				{
					psession->writeMsg((BMessage*)&ssmsg);
				}
			}
		}
	}
	else
	{
		cout << "E_S2S_DB_CHECK_NAME parse failed" << endl;
	}

	return OPER_RES_DONNOT_RETURN;
}

MSG_HANDLE(E_C2S_CREATE_IMAGE)
{
	REQUEST(E_C2S_CREATE_IMAGE)  recv;
	REQUEST(E_S2S_DB_CHECK_NAME)  sendLGS;

	if(recv.ParseFromArray(smsg->GetBodyBuf(),smsg->GetBodySize()))
	{
		LDEBUG(g_PBPrinter.getJsonStream(recv));

		if(smsg->GetSecHead().servertype == SERVER_GG)
		{
			string send_server_msg;
			sendLGS.set_name(recv.name());
			sendLGS.set_portrait(recv.portrait());
			sendLGS.set_id(recv.id());
			sendLGS.set_gg_sid(smsg->GetConnection()->GetSocketFD());
			sendLGS.set_cg_sid(smsg->GetSecHead().csid);
			sendLGS.set_heroid(recv.heroid());
			sendLGS.set_deviceid(recv.deviceid());
			sendLGS.set_gs_sid(g_ServerConfig.jsconfig["server_config"]["zone_id"].asInt());
			sendLGS.SerializeToString(&send_server_msg);

			SMessage ssmsg;
			ssmsg.SetHead(E_S2S_DB_CHECK_NAME,E_S2S_DB_CHECK_NAME, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
			ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

			g_ManagerServer.getGlobalServer()->writeMsg((BMessage*)&ssmsg);
		}
	}
	else
	{
		cout << "E_S2S_DB_CHECK_NAME parse failed" << endl;
	}

	return OPER_RES_DONNOT_RETURN;
}

MSG_HANDLE(E_S2S_HTTP_REQUEST)
{
	Json::Value in;
	Json::Reader jreader;
	std::string inputString((char *)smsg->GetBodyBuf(),smsg->GetBodySize());
	LDEBUG("E_S2S_HTTP_REQUEST content:\n" << inputString.c_str());
	if(!jreader.parse(inputString, in))
	{
		LWARN("E_S2S_HTTP_REQUEST parse json failed!");
		return OPER_RES_DONNOT_RETURN;
	}

	if(in.isMember("http_result"))
	{
		if(in.isMember("msgid") && in["msgid"].asInt() == E_S2S_LOGIN_SID_INFO)
		{
			REQUEST(E_S2S_LOGIN_SID_INFO)  sendGG;
			//这个都要填的，先填写了
			sendGG.set_cg_sid(in["cg_sid"].asInt());
			sendGG.set_gg_sid(in["gg_sid"].asInt());
			string send_server_msg;

			//登录盛大SDK校验结果
			if(in["http_result"].asInt() == 0)	//0表示没错误
			{
				//解析收到的内容
				//TB平台返回的结果是文本格式 不是json格式
				if(in["sdk_key"].asString() == "TB")
				{
					string code = in["http_content"].asString();
					cout << "-------------code:" << code << endl;
					int isok = E_CORRECT;

					do
					{
						if(code == "0")
						{
							LDEBUG("LOGIN sdk_key TB 用户不存在");
							isok = E_ERROR_LOGIN_ERROR;

							break;
						}
						else if(code == "-1")
						{
							LDEBUG("LOGIN sdk_key content error.code:"<<code<<",session:"<<in["pwd"].asString());
							isok = E_ERROR_LOGIN_SDK_CONTENT_ERROR;

							break;
						}
						else
						{
							LDEBUG("sdk result userid:"<<code);

							string send_server_msg;
							string user_name = "_TB_" + code;
							sendGG.set_name(user_name);
							sendGG.set_pwd("123456");
							sendGG.set_sdk_key("TB");

							sendGG.SerializeToString(&send_server_msg);

							SMessage ssmsg;
							ssmsg.SetHead(E_S2S_HTTP_REQUEST,E_S2S_LOGIN_SID_INFO, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().csid);
							ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

							g_ManagerServer.getGlobalServer()->writeMsg((BMessage*)&ssmsg);

							return OPER_RES_DONNOT_RETURN;
						}
					}
					while(false);

					sendGG.set_isok(isok);
				}
				else if(in["sdk_key"].asString() == "HM")
				{
					string code = in["http_content"].asString();
					cout << "-------------code:" << code << endl;
					int isok = E_CORRECT;

					do
					{
						string res = "success&" + in["name"].asString();
						if(code == res)
						{
							LDEBUG("sdk result :"<<code);

							string send_server_msg;
							string user_name = "_HM_" + in["name"].asString();
							sendGG.set_name(user_name);
							sendGG.set_pwd("123456");
							sendGG.set_sdk_key("HM");

							sendGG.SerializeToString(&send_server_msg);

							SMessage ssmsg;
							ssmsg.SetHead(E_S2S_HTTP_REQUEST,E_S2S_LOGIN_SID_INFO, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
							ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

							g_ManagerServer.getGlobalServer()->writeMsg((BMessage*)&ssmsg);

							return OPER_RES_DONNOT_RETURN;
						}
						else
						{
							LDEBUG("LOGIN sdk_key HM 验证失败");
							isok = E_ERROR_LOGIN_SDK_CONTENT_ERROR;

							break;
						}
					}
					while(false);

					sendGG.set_isok(isok);
				}
				else if(in["sdk_key"].asString() == "GP")
				{
					string code = in["http_content"].asString();
					cout << "-------------code:" << code << endl;
					int isok = E_CORRECT;

					do
					{						
						if(code == "-1")
						{
							LDEBUG("LOGIN sdk_key 加密串验证失败");
							isok = E_ERROR_LOGIN_ERROR;

							break;
						}
						else if(code == "-2")
						{
							LDEBUG("LOGIN sdk_key appid error");
							isok = E_ERROR_LOGIN_SDK_CONTENT_ERROR;

							break;
						}
						else if(code == "false")
						{
							LDEBUG("LOGIN sdk_key 登录失败");
							isok = E_ERROR_LOGIN_ERROR;

							break;
						}
						else if(code == "true")
						{
							string send_server_msg;
							string user_name = "_GP_" + in["name"].asString();
							sendGG.set_name(user_name);
							sendGG.set_pwd("123456");
							sendGG.set_sdk_key("GP");

							sendGG.SerializeToString(&send_server_msg);

							SMessage ssmsg;
							ssmsg.SetHead(E_S2S_HTTP_REQUEST,E_S2S_LOGIN_SID_INFO, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
							ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

							g_ManagerServer.getGlobalServer()->writeMsg((BMessage*)&ssmsg);

							return OPER_RES_DONNOT_RETURN;
						}
					}
					while(false);

					sendGG.set_isok(isok);
				}
				else
				{
					Json::Value js_result;
					string result = in["http_content"].asString();
					if(in["sdk_key"].asString() == "JG")
					{
						//先base64解密 再解析json
						result = UrlDecode(Base64Decode(result));					
					}

					LDEBUG("-----Result:"<<result);
					
					if(jreader.parse(result, js_result))
					{
						if(in["sdk_key"].asString() == "SD" || in["sdk_key"].asString() == "GH")
						{
							int code = js_result["code"].asInt();
							int isok = E_CORRECT;
							do
							{
								if(code == 1)
								{
									LDEBUG("LOGIN sdk_key failed!");
									isok = E_ERROR_LOGIN_ERROR;

									break;
								}
								else if(code == 0)
								{
									//验证帐号白名单
									string account = g_ServerConfig.jsconfig["account_white_list"]["account"].asString();
									vector<string> s_;
									Tools::split(account,";",s_);
									if(s_.size() != 0 && s_[0] != "")
									{
										vector<string>::iterator iter = find(s_.begin(),s_.end(),conv2string(js_result["data"]["userid"].asUInt64()));
										if(iter == s_.end())
										{
											isok = E_ERROR_ACCOUNT_WHITE_LIST_LIMIT;
											break;
										}
									}

									string msg = js_result["msg"].asString();
									LDEBUG("sdk result msg:"<<msg);

									string send_server_msg;
									string user_name = "_SD_" + conv2string(js_result["data"]["userid"].asUInt64());
									sendGG.set_name(user_name);
									sendGG.set_pwd("123456");
									sendGG.set_sdk_key("SD");

									sendGG.SerializeToString(&send_server_msg);

									SMessage ssmsg;
									ssmsg.SetHead(E_S2S_HTTP_REQUEST,E_S2S_LOGIN_SID_INFO, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
									ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

									g_ManagerServer.getGlobalServer()->writeMsg((BMessage*)&ssmsg);

									/*
									//发给本地数据服
									sendGG.set_name(in["name"].asString().c_str());
									sendGG.set_pwd(in["pwd"].asString().c_str());
									sendGG.set_id(js_result["data"]["userid"].asInt());

									sendGG.SerializeToString(&send_server_msg);

									SMessage ssmsg;
									ssmsg.SetHead(E_S2S_LOGIN_SID_INFO,E_S2S_LOGIN_SID_INFO, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid);
									ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

									g_ServerList.broadcastMsg(SERVER_DB,(BMessage*)&ssmsg);
									*/
									return OPER_RES_DONNOT_RETURN;
								}
								else
								{
									LDEBUG("LOGIN sdk_key content error.code:"<<code<<",ticket_id:"<<in["pwd"].asString());
									isok = E_ERROR_LOGIN_SDK_CONTENT_ERROR;

									break;
								}
							}
							while(false);

							sendGG.set_isok(isok);
						}
						else if(in["sdk_key"].asString() == "KY")
						{
							int code = js_result["code"].asInt();
							int isok = E_CORRECT;
							do
							{
								if(code == 0)
								{
									string msg = js_result["msg"].asString();
									LDEBUG("sdk result msg:"<<msg);

									string send_server_msg;
									string user_name = "_KY_" + js_result["data"]["guid"].asString();
									sendGG.set_name(user_name);
									sendGG.set_pwd("123456");
									sendGG.set_sdk_key("KY");

									sendGG.SerializeToString(&send_server_msg);

									SMessage ssmsg;
									ssmsg.SetHead(E_S2S_HTTP_REQUEST,E_S2S_LOGIN_SID_INFO, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
									ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

									g_ManagerServer.getGlobalServer()->writeMsg((BMessage*)&ssmsg);
									/*
									//发给本地数据服
									sendGG.set_name(in["name"].asString().c_str());
									sendGG.set_pwd(in["pwd"].asString().c_str());
									sendGG.set_id(atoi(js_result["data"]["guid"].asString().c_str()));

									sendGG.SerializeToString(&send_server_msg);

									SMessage ssmsg;
									ssmsg.SetHead(E_S2S_LOGIN_SID_INFO,E_S2S_LOGIN_SID_INFO, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid);
									ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

									g_ServerList.broadcastMsg(SERVER_DB,(BMessage*)&ssmsg);
									*/
									return OPER_RES_DONNOT_RETURN;
								}
								else if(code == 6)
								{
									LDEBUG("LOGIN sdk_key KY 用户不存在");
									isok = E_ERROR_LOGIN_ERROR;

									break;
								}
								else
								{
									LDEBUG("LOGIN sdk_key content error.code:"<<code<<",ticket_id:"<<in["pwd"].asString());
									isok = E_ERROR_LOGIN_SDK_CONTENT_ERROR;

									break;
								}
							}
							while(false);

							sendGG.set_isok(isok);
						}
						else if(in["sdk_key"].asString() == "XY")
						{
							int code = js_result["ret"].asInt();
							int isok = E_CORRECT;
							do
							{
								if(code == 0)
								{
									//string msg = js_result["msg"].asString();
									//LDEBUG("sdk result msg:"<<msg);

									string send_server_msg;
									//string user_name = "_XY_" + js_result["data"]["userid"].asString();
									string user_name = "_XY_" + in["name"].asString();
									sendGG.set_name(user_name);
									sendGG.set_pwd("123456");
									sendGG.set_sdk_key("XY");

									sendGG.SerializeToString(&send_server_msg);

									SMessage ssmsg;
									ssmsg.SetHead(E_S2S_HTTP_REQUEST,E_S2S_LOGIN_SID_INFO, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
									ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

									g_ManagerServer.getGlobalServer()->writeMsg((BMessage*)&ssmsg);

									return OPER_RES_DONNOT_RETURN;
								}
								else if(code == 997)
								{
									LDEBUG("LOGIN sdk_key XY 用户不存在.token:"<<in["pwd"].asString());
									isok = E_ERROR_LOGIN_ERROR;

									break;
								}
								else
								{
									LDEBUG("LOGIN sdk_key content error.code:"<<code<<",ticket_id:"<<in["pwd"].asString());
									isok = E_ERROR_LOGIN_SDK_CONTENT_ERROR;

									break;
								}
							}
							while(false);

							sendGG.set_isok(isok);
						}
						else if(in["sdk_key"].asString() == "AS")
						{
							int code = js_result["status"].asInt();
							int isok = E_CORRECT;
							do
							{
								if(code == 0)
								{
									string send_server_msg;
									string user_name = "_AS_" + conv2string(js_result["userid"].asInt());
									sendGG.set_name(user_name);
									sendGG.set_pwd("123456");
									sendGG.set_sdk_key("AS");

									sendGG.SerializeToString(&send_server_msg);

									SMessage ssmsg;
									ssmsg.SetHead(E_S2S_HTTP_REQUEST,E_S2S_LOGIN_SID_INFO, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
									ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

									g_ManagerServer.getGlobalServer()->writeMsg((BMessage*)&ssmsg);

									return OPER_RES_DONNOT_RETURN;
								}
								else if(code == 2)
								{
									LDEBUG("LOGIN sdk_key AS 用户不存在.token:"<<in["pwd"].asString());
									isok = E_ERROR_LOGIN_ERROR;

									break;
								}
								else
								{
									LDEBUG("LOGIN sdk_key content error.code:"<<code<<",token:"<<in["pwd"].asString());
									isok = E_ERROR_LOGIN_SDK_CONTENT_ERROR;

									break;
								}
							}
							while(false);

							sendGG.set_isok(isok);
						}
						else if(in["sdk_key"].asString() == "JG")
						{
							int isok = E_CORRECT;
							do
							{
								string send_server_msg;
								string user_name = "_JG_" + conv2string(js_result["userid"].asInt());
								sendGG.set_name(user_name);
								sendGG.set_pwd("123456");
								sendGG.set_sdk_key("JG");

								sendGG.SerializeToString(&send_server_msg);

								SMessage ssmsg;
								ssmsg.SetHead(E_S2S_HTTP_REQUEST,E_S2S_LOGIN_SID_INFO, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
								ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

								g_ManagerServer.getGlobalServer()->writeMsg((BMessage*)&ssmsg);

								return OPER_RES_DONNOT_RETURN;							
							}
							while(false);

							sendGG.set_isok(isok);
						}
						else
						{
							sendGG.set_isok(E_ERROR_LOGIN_SDK_ERROR);
						}
					}
					else
					{
						//网页内容json转换出错，回复给网关
						sendGG.set_isok(E_ERROR_LOGIN_SDK_CONTENT_ERROR);
					}
				}
			}
			else
			{
				//http 请求出错，回复给网关
				sendGG.set_isok(E_ERROR_LOGIN_SDK_CONTENT_ERROR);
			}

			sendGG.SerializeToString(&send_server_msg);

			SMessage ssmsg;
			ssmsg.SetHead(E_S2S_LOGIN_SID_INFO,E_S2S_LOGIN_SID_INFO, g_ServerConfig.cur_servertype, smsg->GetSecHead().csid,smsg->GetSecHead().ssid);
			ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

			Session * psession = (Session *)g_ConnectionMgt.GetSocket(sendGG.gg_sid());
			if(NULL == psession)
			{
				LWARN("Message ssid " << sendGG.gg_sid() << " not exist");
			}
			else
			{
				psession->writeMsg((BMessage*)&ssmsg);
			}
		}
	}
	else
	{
		LWARN("E_S2S_HTTP_REQUEST have no http_result!");
	}
	return OPER_RES_DONNOT_RETURN;
}

MSG_HANDLE(E_S2S_REQ_GS_SERVERID)
{
	if(smsg->GetSecHead().servertype == SERVER_GG)
	{
		if(g_ManagerServer.get_load_costume_piece() == false)
		{
			REQUEST(E_S2S_REQ_GS_SERVERID) sendGS;

			string send_server_msg;
			vector<int32_t> list;
			g_ServerList.getActiveServerID(SERVER_GS,list);
			for(size_t i=0;i<list.size();++i)
			{
				sendGS.add_gs_serverid(list[i]);
			}
			sendGS.set_isok(1);
			sendGS.SerializeToString(&send_server_msg);

			SMessage ssmsg;
			ssmsg.SetHead(0,E_S2S_LOAD_COSTUME_PIECE, g_ServerConfig.cur_servertype,0,0);
			ssmsg.SetContent((uint8_t*)send_server_msg.c_str(),send_server_msg.length());

			g_ServerList.broadcastMsg(SERVER_GS,(BMessage*)&ssmsg);

			g_ManagerServer.set_load_costume_piece(true);
		}
	}

	return OPER_RES_DONNOT_RETURN;
}

MSG_HANDLE(E_S2S_ONLINE_REDUCE)
{
	if(smsg->GetSecHead().servertype == SERVER_GG)
	{
		//g_ManagerServer.update_online_num( -1 );
	}

	return OPER_RES_DONNOT_RETURN;
}

MSG_HANDLE(E_S2S_ONLINE_ADD)
{
	if(smsg->GetSecHead().servertype == SERVER_GG)
	{
		//g_ManagerServer.update_online_num( 1 );
	}

	return OPER_RES_DONNOT_RETURN;
}

MSG_HANDLE(E_S2S_GET_ONLINE_NUM)
{

	RESPOND(E_S2S_GET_ONLINE_NUM) resp;
	resp.set_zoneid(g_ServerConfig.zone_id);
	resp.set_num(g_ManagerServer.get_online_num());
	resp.SerializeToString(&content);

	return OPER_RES_STRING;
}

MSG_HANDLE(E_C2S_MODIFY_NICKNAME)
{
	//转发lgs
	if(smsg->GetSecHead().servertype == SERVER_GS)
	{
		if(netPeer)
		{
			SMessage ssmsg;
			ssmsg.SetHead(0,E_C2S_MODIFY_NICKNAME,g_ServerConfig.cur_servertype,netPeer->getPeerID(),0);
			ssmsg.SetContent((uint8_t*)smsg->GetBodyBuf(),smsg->GetBodySize());

			g_ManagerServer.getGlobalServer()->writeMsg((BMessage*)&ssmsg);
		}
	}
	else if(smsg->GetSecHead().servertype == SERVER_LGS)
	{
		SMessage ssmsg;
		ssmsg.SetHead(0,E_S2S_MODIFY_NICKNAME, g_ServerConfig.cur_servertype,smsg->GetSecHead().csid,0);
		ssmsg.SetContent((uint8_t*)smsg->GetBodyBuf(),smsg->GetBodySize());
		CNetPeer* netPeer_ = g_NetPeerMgr.getNetPeer(smsg->GetSecHead().csid);
		if(netPeer_)
		{
			netPeer_->sendMsgToGS((BMessage*)&ssmsg);
		}
	}

	return OPER_RES_DONNOT_RETURN;
}

MSG_HANDLE(E_S2S_NOTIFY_SERVER_STATE)
{
	REQUEST(E_S2S_NOTIFY_SERVER_STATE)  recv;
	if(recv.ParseFromArray(smsg->GetBodyBuf(),smsg->GetBodySize()))
	{
		LDEBUG(g_PBPrinter.getJsonStream(recv));
		g_ManagerServer.add_online();
		if(g_ManagerServer.gate_num == g_ManagerServer.get_online_c())
		{
			g_ManagerServer.update_online_num(recv.online_num());

			//定时向log服保存在线人数信息
			REQUEST(E_S2S_NOTIFY_SERVER_STATE) pro_log;
			pro_log.set_online_num( g_ManagerServer.get_online_num());
			pro_log.set_serverid(g_ServerConfig.cur_serverid);
			string send_log = "";
			pro_log.SerializeToString(&send_log);

			SMessage ssmsg;
			ssmsg.SetHead(0,E_S2S_NOTIFY_SERVER_STATE, g_ServerConfig.cur_servertype,0,0);
			ssmsg.SetContent((uint8_t*)send_log.c_str(),send_log.length());

			g_ServerList.broadcastMsg(SERVER_LOG,(BMessage*)&ssmsg);

			cout << "---- 当前服务器在线人数："<< g_ManagerServer.get_online_num() << " ---" << endl;

		}
		else
		{
			g_ManagerServer.update_online_num(recv.online_num());
		}

	}

	return OPER_RES_DONNOT_RETURN;
}

MSG_HANDLE(E_S2S_GUILD_CREATE)
{
	//转发lgs
	if(smsg->GetSecHead().servertype == SERVER_GS)
	{
		if(netPeer)
		{
			SMessage ssmsg;
			ssmsg.SetHead(0,E_S2S_GUILD_CREATE,g_ServerConfig.cur_servertype,netPeer->getPeerID(),0);
			ssmsg.SetContent((uint8_t*)smsg->GetBodyBuf(),smsg->GetBodySize());

			g_ManagerServer.getGlobalServer()->writeMsg((BMessage*)&ssmsg);
		}
	}
	else if(smsg->GetSecHead().servertype == SERVER_LGS)
	{
		SMessage ssmsg;
		ssmsg.SetHead(0,E_S2S_GUILD_CREATE, g_ServerConfig.cur_servertype,smsg->GetSecHead().csid,0);
		ssmsg.SetContent((uint8_t*)smsg->GetBodyBuf(),smsg->GetBodySize());
		CNetPeer* netPeer_ = g_NetPeerMgr.getNetPeer(smsg->GetSecHead().csid);
		if(netPeer_)
		{
			netPeer_->sendMsgToGS((BMessage*)&ssmsg);
		}
	}

	return OPER_RES_DONNOT_RETURN;
}

MSG_HANDLE(E_S2S_GUILD_MODIFY_NAME)
{
	//转发lgs
	if(smsg->GetSecHead().servertype == SERVER_GS)
	{
		if(netPeer)
		{
			SMessage ssmsg;
			ssmsg.SetHead(0,E_S2S_GUILD_MODIFY_NAME,g_ServerConfig.cur_servertype,netPeer->getPeerID(),0);
			ssmsg.SetContent((uint8_t*)smsg->GetBodyBuf(),smsg->GetBodySize());

			g_ManagerServer.getGlobalServer()->writeMsg((BMessage*)&ssmsg);
		}
	}
	else if(smsg->GetSecHead().servertype == SERVER_LGS)
	{
		SMessage ssmsg;
		ssmsg.SetHead(0,E_S2S_GUILD_MODIFY_NAME, g_ServerConfig.cur_servertype,smsg->GetSecHead().csid,0);
		ssmsg.SetContent((uint8_t*)smsg->GetBodyBuf(),smsg->GetBodySize());
		CNetPeer* netPeer_ = g_NetPeerMgr.getNetPeer(smsg->GetSecHead().csid);
		if(netPeer_)
		{
			netPeer_->sendMsgToGS((BMessage*)&ssmsg);
		}
	}

	return OPER_RES_DONNOT_RETURN;
}


MSG_HANDLE(E_C2S_USE_CDKEY)
{
	REQUEST(E_C2S_USE_CDKEY)  recv;
	RESPOND(E_C2S_USE_CDKEY)  resp;

	if(!recv.ParseFromArray(smsg->GetBodyBuf(),smsg->GetBodySize()))
	{
		protos::rep_common respd;
		respd.set_isok(E_ERROR_PROTO_PARSE_ERROR);
		respd.SerializeToString(&content);
		return OPER_RES_STRING;
	}

	/*
	int64_t roleid = recv.roleid();
	string cdkey = recv.cdkey();

	//
	// Don't change the following code
	//
	char buf[2048];
	GoldAwardAuthenInfoReqDef *p = (GoldAwardAuthenInfoReqDef *)buf;
	MsgHeadDef *pHead = &p->msgHead;
	GoldAwardAuthenReqDef *pMsg = &p->msgBody;
	pHead->msgType = htonl(GOLDAWARDAUTHENREQ);
	pHead->msgLen = htonl(sizeof(GoldAwardAuthenInfoReqDef));
	pHead->callTime = htonl(time(NULL));
	pHead->version = htonl(MSGVERSION);

	if(g_ManagerServer.pRmGetSotsUniqueId)
	{
		g_ManagerServer.pRmGetSotsUniqueId(pHead->msgId, 33);		  // Get unique string
		g_ManagerServer.pRmGetSotsUniqueId(pMsg->orderId, 33);		// Get another unique string
	}
	cout << " gameid:"<< g_ManagerServer.cltinfo.gameId << endl;
	cout << " groupid:" << g_ManagerServer.cltinfo.groupId << endl;
	cout << " areaid:"<< g_ManagerServer.cltinfo.areaId << endl;
	cout << " hostid:"<< g_ManagerServer.cltinfo.hostId << endl;

	pMsg->gameId = htonl(g_ManagerServer.cltinfo.gameId);		  // Use default value
	pMsg->groupId = htonl(g_ManagerServer.cltinfo.groupId);		// Use default value
	pMsg->areaId = htonl(g_ManagerServer.cltinfo.areaId);		  // Use default value
	pMsg->hostId = htonl(g_ManagerServer.cltinfo.hostId);		  // Use default value

	pMsg->awardType = htonl(3);					// awardType=3 means you need set awardNum
	pMsg->uidType = htonl(2);					  // User account type. 2-Number account

	//
	// You must set these field
	//
	strcpy(pMsg->awardNum,cdkey.c_str());	               // Set coupon number
	pMsg->userGrade = htonl(0);			                     // Set user account level
	pMsg->awardGrade = htonl(0);		                     // Which level item do you want
	strcpy(pMsg->userId,conv2string(roleid).c_str());		 // Set user account
	strcpy(pMsg->roleName, "0");	                       // Set role name
	strcpy(pMsg->authenTime, Tools::get_datestring_by_timestamp().c_str());	 // Set current time
	strcpy(pMsg->roleProperty, "0");                     // Set role property like "male#warrior"
	strcpy(pMsg->batchId, "0");	                         // Set batch id
	strcpy(pMsg->endpointIp, "1.2.3.4");			           // Set player's IP address

	cout << "-----awardNum:"<< pMsg->awardNum << endl;
	cout << "time:"<< pMsg->authenTime << endl;
	cout << "property:" << pMsg->roleProperty << endl;

	int len = sizeof(GoldAwardAuthenInfoReqDef);
	if(g_ManagerServer.pRmAsynCall)
	{
		int	hRpc = g_ManagerServer.pRmAsynCall("SVCAWARD", buf, len, 0);
		if (hRpc < 1)
		{
			// handle error
			LDEBUG("-----pRmAsynCall hRpc:"<< hRpc);
		}
		else
		{
			DelayDataMsg s_data(smsg->GetSecHead().csid, smsg->GetConnection()->GetSocketFD(),(uint16_t)E_S2S_USE_CDKEY, string((char*)smsg->GetBodyBuf(),smsg->GetBodySize()),SERVER_GG);

			g_ManagerServer.m_delaymsgs.insert(ManagerServer::DelayDataMsgMap::value_type(roleid,s_data));
		}
	}
	*/
	
	//转发lgs
	if(smsg->GetSecHead().servertype == SERVER_GG)
	{
		if(netPeer)
		{
			SMessage ssmsg;
			ssmsg.SetHead(0,E_S2S_USE_CDKEY,g_ServerConfig.cur_servertype,netPeer->getPeerID());
			ssmsg.SetContent((uint8_t*)smsg->GetBodyBuf(),smsg->GetBodySize());

			g_ManagerServer.getGlobalServer()->writeMsg((BMessage*)&ssmsg);
		}
	}
	

	return OPER_RES_DONNOT_RETURN;
}

MSG_HANDLE(E_S2S_USE_CDKEY)
{
	if(smsg->GetSecHead().servertype == SERVER_LGS ||
		 smsg->GetSecHead().servertype == SERVER_GG)
	{
		RESPOND(E_S2S_USE_CDKEY)  recv;
		if(recv.ParseFromArray(smsg->GetBodyBuf(),smsg->GetBodySize()))
		{
			LDEBUG(g_PBPrinter.getJsonStream(recv));
			if(recv.isok() != E_CORRECT)
			{
				//返回GG
				if(netPeer)
				{
					string resp_msg;
					RESPOND(E_C2S_USE_CDKEY) respGG;
					respGG.set_isok(recv.isok());
					respGG.SerializeToString(&resp_msg);

					SMessage ssmsg;
					ssmsg.SetHead(E_C2S_USE_CDKEY,E_C2S_USE_CDKEY,g_ServerConfig.cur_servertype,netPeer->getPeerID(),0);
					ssmsg.SetContent((uint8_t*)resp_msg.c_str(),resp_msg.length());
					netPeer->sendMsgToGG((BMessage*)&ssmsg);
				}
			}
			else
			{
				//将消息返回给gs 收取奖励
				if(netPeer)
				{
					SMessage ssmsg;
					ssmsg.SetHead(0,E_S2S_USE_CDKEY,g_ServerConfig.cur_servertype,netPeer->getPeerID(),0);
					ssmsg.SetContent((uint8_t*)smsg->GetBodyBuf(),smsg->GetBodySize());
					g_ServerList.sendMsgToXX(SERVER_GS,recv.roleid(),(BMessage*)&ssmsg);
				}
			}
		}
	}

	return OPER_RES_DONNOT_RETURN;
}

void HandleSotsCallback(ULONG handle, char* pServiceName, char *pData, INT32 len)
{
	if(strcmp(pServiceName, SVCSOTSSYSTEM) == 0)
	{
		char msgInfo[2048];

		SotsClientInfoDef *p = (SotsClientInfoDef *)msgInfo;
		memcpy(msgInfo,pData,len);
		if (p->msgHead.msgType == TYPESOTSCLIENTINFO)
		{
			memcpy(&g_ManagerServer.cltinfo,&p->msgBody,sizeof(SOTSCLIENTINFO));
		}
		return ;
	}
	else if(strcmp(pServiceName, "SVCAWARD") == 0)
	{
		MsgHeadDef *pHead = (MsgHeadDef *)pData;
		size_t msgType = ntohl(pHead->msgType);
		if(msgType == GOLDAWARDAUTHENRES)
		{
			GoldAwardAuthenInfoResDef *p = (GoldAwardAuthenInfoResDef *)pData;
			int64_t roleid = atol(p->msgBody.userId);
			LDEBUG("-----SVCAWARD roleid:"<< roleid);
			ManagerServer::DelayDataMsgMap::iterator it  = g_ManagerServer.m_delaymsgs.find(roleid);
			if(it == g_ManagerServer.m_delaymsgs.end())
			{
				LDEBUG("-----m_delaymsgs not found");
				return ;
			}
			Session * psession = (Session *)g_ConnectionMgt.GetSocket(it->second.m_ggsid);
			if(psession == NULL)
			{
				LDEBUG("-----SVCAWARD session == NULL");
				return ;
			}

			REQUEST(E_C2S_USE_CDKEY) recv;
			if(!recv.ParseFromArray((uint8_t*)it->second.m_content.c_str(), it->second.m_content.length()))
			{
				return ;
			}

			RESPOND(E_S2S_USE_CDKEY) resp;
			string resp_msg = "";

			int r = ntohl(p->msgBody.result);

			int32_t isok = 1;
			LDEBUG("-----Ump result:"<< r);
			if ( r == -1)
			{
				isok = E_ERROR_CDKEYBAG_NOT_FOUND;
			}
			else if( r == -20)
			{
				isok = E_ERROR_CDKEY_OUT_OF_DATE;
			}
			else if( r == -21)
			{
				isok = E_ERROR_BAG_HAD_USED;
			}
			else if( r == -22)
			{
				isok = E_ERROR_CDKEY_HAD_USED;
			}
			else if( r == -23)
			{
				isok = E_ERROR_CDKEY_NOT_FOUND;
			}
			else if (r == 0 || r == 1)
			{
				//
				// Get ItemInfo.
				//
				int len = 0;
				for (size_t j=0; j<ntohl(p->msgBody.itemKindCount); j++)
				{
					char * tmpBuf = p->msgBody.itemInfo + len;
					ItemInfoDef *q = (ItemInfoDef *)tmpBuf;

					char tmp[200];
					sprintf(tmp, "(%s, %d, %s)", q->itemId, ntohl(q->itemNum), q->itemAttr);
					cout << "------tmp:"<< tmp << endl;

					int32_t itemid = atoi(q->itemId);
					int32_t itemnum = ntohl(q->itemNum);
					int32_t itemtype = 2;
					//获取道具类型
					/*
					char *p = strstr(q->itemAttr, "T:");
					if (p != NULL)
					{
						itemtype = atoi(p+2);
					}
					*/
					LDEBUG("-----Itemid:"<< itemid <<"--Itemnum:"<< itemnum <<"--Itemtype:"<< itemtype);

					//
					// Handle ItemInfo
					//
					protos::Bag_item* p_item = resp.add_item();
					if(p_item)
					{
						p_item->set_type(itemtype);
						p_item->set_id(itemid);
						p_item->set_num(itemnum);
					}

					int itemLen ;
					if ((ntohl(q->itemAttrLen) % 4) == 0)
					{
						itemLen = ntohl(q->itemAttrLen) ;
					}
					else
					{
						itemLen = (ntohl(q->itemAttrLen)/4 + 1) * 4;
					}

					len += sizeof(ItemInfoDef) - 4 + itemLen;

				}

				//
				// Send ack to UMP server.
				//
				char buf[2048];
				GoldAwardAckInfoDef *awardAck = (GoldAwardAckInfoDef *)buf;
				awardAck->msgHead.msgType = htonl(4);
				awardAck->msgHead.callTime = htonl(time(NULL));
				awardAck->msgHead.version = p->msgHead.version;
				awardAck->msgHead.msgLen = htonl(sizeof(awardAck));

				g_ManagerServer.pRmGetSotsUniqueId(awardAck->msgHead.msgId, 33);
				strcpy(awardAck->msgBody.orderId, p->msgBody.orderId);  // Copy orderId from UMP server
				strcpy(awardAck->msgBody.userId, p->msgBody.userId);	// Copy userId from UMP server

				awardAck->msgBody.uidType = htonl(2);			// Set user account type
				awardAck->msgBody.itemKindCount = 0;			// Set default value
				awardAck->msgBody.itemInfoLen = 0;				// Set default value

				awardAck->msgBody.confirm = htonl(0);			// 0-succ, other-fail
				strcpy(awardAck->msgBody.description, "OK");	// OK or FAIL

				len = sizeof(GoldAwardAckInfoDef) - 4 + 0;		// Caculate message size
				awardAck->msgHead.msgLen = htonl(len);
				awardAck->msgHead.msgType = htonl(GOLDAWARDACK);
				int hRpc = g_ManagerServer.pRmAsynCall(SVCAWARDITEM, (char *)awardAck, len, 4);
				cout << "-------hRpc:"<<hRpc << endl;
				if (hRpc < 1)
				{
					// error
				}
			}
			else
			{
				isok = E_ERROR_CDKEY;
			}

			resp.set_isok(isok);
			resp.set_roleid(roleid);
			resp.SerializeToString(&resp_msg);
			SMessage* smsg = (SMessage*) psession->CreateMessage();
			smsg->SetHead(it->second.m_msgtype, it->second.m_msgtype, it->second.m_servertype, it->second.m_csid, it->second.m_ggsid);
			smsg->SetContent((uint8_t*)resp_msg.c_str(), resp_msg.length());
			smsg->Encode();

			g_JobQueue.submitjob((BMessage*)smsg);	//放入到工作队列里
			//删除m_delaymsgs 信息
			g_ManagerServer.m_delaymsgs.erase(roleid);
		}
	}
}

MSG_HANDLE(E_GM_REFRESH_ACCOUNT_WHITE_LIST)
{
	//重新加载
	g_ServerConfig.reload_server_config();

	protos::rep_common resp;
	resp.set_isok(1);
	resp.SerializeToString(&content);
	return OPER_RES_STRING;
}
