#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>
#include <curl/easy.h>

#include "download.h"

const char WINDOWS_AGENT[] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/88.0.4324.152 Safari/537.36 Edge/12.246";
const char UBUNTU_AGENT[] = "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:15.0) Gecko/20100101 Firefox/15.0.1";
const char LINUX_AGENT[] = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/88.0.4324.152 Safari/537.36";
const char IPHONE_AGENT[] = "iPhone; CPU iPhone OS 12_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/12.0 Mobile/15E148 Safari/604.1";
const char ANDROID_AGENT[] = "Mozilla/5.0 (Linux; Android 8.0.0; SM-G960F Build/R16NW) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/62.0.3202.84 Mobile Safari/537.36";

const char* random_agent() {
	int n = rand() % 5;	
	if(n == 0) return WINDOWS_AGENT;
	else if(n == 1) return UBUNTU_AGENT;
	else if(n == 2) return LINUX_AGENT;
	else if(n == 3) return IPHONE_AGENT;
	else return ANDROID_AGENT;
}


static size_t
write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  memory_struct_t* mem = (memory_struct_t*)userp;
 
  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if(ptr == NULL) {
    /* out of memory! */ 
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }
 
  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;
}

int download_file(const char* url, memory_struct_t* chunk) {
	CURL *curl_handle;
	CURLcode res;

	chunk->memory = malloc(1);  /* will be grown as needed by the realloc above */ 
	chunk->size = 0;    /* no data at this point */ 

	curl_global_init(CURL_GLOBAL_ALL);

	/* init the curl session */ 
	curl_handle = curl_easy_init();

	/* specify URL to get */ 
	curl_easy_setopt(curl_handle, CURLOPT_URL, url);

	/* send all data to this function  */ 
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_memory_callback);

	/* we pass our 'chunk' struct to the callback function */ 
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)chunk);

	/* some servers don't like requests that are made without a user-agent
	field, so we provide one */ 
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, random_agent());

	/* get it! */ 
	res = curl_easy_perform(curl_handle);

	/* check for errors */ 
	/*if(res != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n",
		curl_easy_strerror(res));
	}
	else {
	//	* Now, our chunk.memory points to a memory block that is chunk.size
	//	* bytes big and contains the remote file.
	//	*
	//	* Do something nice with it!
		printf("%lu bytes retrieved\n", (unsigned long)chunk->size);
	}*/

	/* cleanup curl stuff */ 
	curl_easy_cleanup(curl_handle);

	/* we're done with libcurl, so clean it up */ 
	curl_global_cleanup();

	if(res != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n",
		curl_easy_strerror(res));
		return DOWNLOAD_ERROR;
	}
	return DOWNLOAD_OK;
}
