
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "dk_core.h"

#define HTTP_HOST "localhost"
#define HTTP_PORT 60006
#define HTTP_POST_DATA "aaaaaaaaaaaaaaaaaaaaa"
#define PARALLEL 1
#define TIMEOUT 3600
#define HTTP_CONNS 10
using namespace std;

DKEventThread *ev_thread;
DKHttpClient *s_http_client;

static bool http_run_once();

static int start_time;
static int end_time;
static long send_http_reqs = 0;
static long recv_http_resps = 0;

class MyHttpRequest: public DKHttpRequest {
  virtual void HandleResponse(struct evhttp_request *req, void *arg) {
    DKHttpRequest::DebugResponse(req);
    recv_http_resps++;
    http_run_once();
  }
};

static bool http_run_once() {
  static int http_run_init = 0;
  static int last_secs = 0;
  if (!ev_thread)
    return false;

  if (!s_http_client) {
    s_http_client = new DKHttpClient();
    if (!s_http_client)
      return false;
  
    if (!s_http_client->Init(ev_thread->get_base(), HTTP_HOST, HTTP_PORT, HTTP_CONNS))
      return false;

    s_http_client->SetTimeout(HTTP_CONNS);
  }

  int parallel = 1;
  if (http_run_init == 0) {
    parallel = PARALLEL; 
    start_time = time(0);
    http_run_init = 1;
  }

  for (int i = 0; i < parallel; i++) {
    send_http_reqs++;
    if (send_http_reqs % 10000 == 0) {
      end_time = time(0);
      int secs = end_time - start_time;
      if (secs > last_secs) {
        last_secs = secs;
        fprintf(stderr, "send_reqs:%d, recv_reqs:%d, secs:%d\n",
                send_http_reqs, recv_http_resps, secs);
      }
    }

    MyHttpRequest *req = new MyHttpRequest();
    if (!req)
      return false;

    if (!req->Init()) {
      delete req;
      return false;
    }

    req->AddHeader("Host", HTTP_HOST);
    req->AddHeader("Connection", "keep-alive");

    //int post_data_len = strlen(HTTP_POST_DATA);
    //char temp[16];
    //sprintf(temp, "%d", post_data_len);
    
    //req->AddHeader("Content-length", temp);
    //req->AddPostData((void *)HTTP_POST_DATA, post_data_len);

    if (!s_http_client->SendRequest(req, EVHTTP_REQ_GET, "/dump")) {
      delete req;
    }
  }
}

int main(int argc, char **argv) {
  ev_thread = new DKEventThread();

  if (!ev_thread || !ev_thread->Init()) {
    fprintf(stderr, "new DKBaseServer Init error\n");
    return 1;
  }

  signal(SIGPIPE, SIG_IGN);
  signal(SIGHUP, SIG_IGN);

  fprintf(stderr, "begin http_load run ....");
  http_run_once();
  ev_thread->ThreadRoutine();

  delete ev_thread;
}
