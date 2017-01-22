
#include <curl/curl.h>

#include "protocol_define.h"
#include "cmd_factory.h"

#include "s2s_dnl.pb.h"
#include "server_list.h"
#include "../http_server.h"

using namespace std;


struct MemoryStruct
{
	char *memory;
	size_t size;
};

size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	struct MemoryStruct *mem = (struct MemoryStruct *)userp;
	mem->memory = (char*)realloc(mem->memory, mem->size + realsize + 1);
	if(mem->memory == NULL)
	{
		std::cerr << "not enough memory (realloc returned NULL)" << std::endl;
		return 0;
	}

	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}

MSG_HANDLE(E_S2S_HTTP_REQUEST)	//http请求
{
	//smsg内容
	cout<<" E_S2S_HTTP_REQUEST get msg : "<<smsg->GetBodyBuf()<<endl;
	try
	{
		Json::Value in;
		std::string inputString((char *)smsg->GetBodyBuf(),smsg->GetBodySize());
		if(!Json::Reader().parse(inputString, in))	//无法parse返回错误
		{
			LWARN("E_S2S_HTTP_REQUEST parse json failed!");
			content = "{\"http_result\":-1}";
			return OPER_RES_STRING;
		}

		if(!in.isMember("url") || !in.isMember("method"))	//没有url等错误
		{
			LWARN("E_S2S_HTTP_REQUEST failed! miss url or method param!");
			content = "{\"http_result\":-2}";
			return OPER_RES_STRING;
		}
		const std::string &url = in["url"].asString();	//获取url
		const std::string &method = in["method"].asString();	//返回method

		{
			//发送get请求
			CURL *curl;
			CURLcode res;

			struct MemoryStruct chunk;
			chunk.memory = (char*)malloc(1); 
			chunk.size = 0;

			int result = 0;	//0 是没有错误。其他的有错误

			curl = curl_easy_init();//get a curl handle
			if(curl) 
			{

				if(in.isMember("headers"))
				{
					struct curl_slist *headers=NULL;  // init to NULL is important 
					//headers = curl_slist_append(headers, "charset=UTF-8");
					for(int i = 0;i < (int)in["headers"].size();++i)
					{
						LDEBUG("headers[" << i << "]:" << in["headers"][i].asString().c_str());
				    	headers = curl_slist_append(headers, in["headers"][i].asString().c_str());
					}
					curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
				}

				curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

				// 设置重定向的最大次数
				curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5);

				// 设置301、302跳转跟随location
				curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

				char * tmp = NULL;
				if("POST" == method)
				{
					int plen = in["postdata"].asString().length();
					tmp= (char *)malloc(plen+1);
					assert(tmp);
					memcpy(tmp,in["postdata"].asString().c_str(),plen);
					curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST,"POST");
					curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,plen);
					curl_easy_setopt(curl, CURLOPT_POSTFIELDS, tmp);    // 指定post内容
				}
		        
				curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30 );//接收数据时超时设置，如果10秒内数据未接收完，直接退出
				curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20);//连接超时，这个数值如果设置太短可能导致数据请求不到就断开了
				//这个很重要，curl 有个重用机制，访问的链接不会立即释放，如果访问过于平凡的可能会导致服务器端口被沾满，无法访问的情况
				curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1); 

				curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);	
				res = curl_easy_perform(curl);
				
				LDEBUG("curl res = " << res);
				if(res != CURLE_OK)//Check for errors
				{
					result = res;	//请求失败
					LDEBUG("##curl_easy_perform error" << curl_easy_strerror(res));
				}
				if(tmp)
					free(tmp);
				curl_easy_cleanup(curl);//always cleanup

			}
			else
			{
				result = 1;	//curl 获得失效
			}

			//对方请求的 原样返回，再附上http_result 和 http_content
			in["http_result"] = result;
			//把结果放回队列
			if(0 == result)
			{
				//把结果输出一下
				LDEBUG("curl get:" << chunk.memory);
				in["http_content"] = chunk.memory;
			}
			else
			{
				LWARN("curl get ERROR:" << result);
			}

			//返回
			content = in.toStyledString();
			
			cout<<" E_S2S_HTTP_REQUEST ret msg : "<< content <<endl;
			
			if(chunk.memory)
				free(chunk.memory);

			return OPER_RES_STRING;
		}
	}
	catch(std::runtime_error &e)
	{
		LWARN("ERROR,E_S2S_HTTP_REQUEST , runtime_error(" << e.what() << ")");
	}
	
	return OPER_RES_DONNOT_RETURN;
 }
