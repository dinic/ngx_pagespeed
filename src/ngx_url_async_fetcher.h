/*
 * Copyright 2012 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: x.dinic@gmail.com(Junmin Xiong)
//
// Fetch the resources asynchronously in Nginx. The fetcher is called in
// the rewrite thread.
//
// It can communicate with Nginx by pipe. One pipe for one fetcher.
// When new url fetch comes, Fetcher will add it to the pending queue and
// notify the Nginx thread to start the Fetch event. All the events are hooked
// in the main thread's epoll structure.

#ifndef NET_INSTAWEB_NGX_URL_ASYNC_FETCHER_H_
#define NET_INSTAWEB_NGX_URL_ASYNC_FETCHER_H_

extern "C" {
  #include <ngx_config.h>
  #include <ngx_core.h>
}

#include <vector>
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/pool.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/http/public/url_pollable_async_fetcher.h"


namespace net_instaweb {

class AsyncFetch;
class MessageHandler;
class Statistics;
class NgxFetch;
class Variable;

class NgxUrlAsyncFetcher : public UrlAsyncFetcher {
  public:
    NgxUrlAsyncFetcher(const char* proxy, ngx_pool_t* pool,
        ngx_msec_t resolver_timeout, ngx_msec_t fetch_timeout,
        ngx_resolver_t* resolver, MessageHandler* handler);
    NgxUrlAsyncFetcher(const char* proxy, ngx_log_t* log,
        ngx_msec_t resolver_timeout, ngx_msec_t fetch_timeout,
        ngx_resolver_t* resolver, ThreadSystem* thread_system,MessageHandler* handler);
    NgxUrlAsyncFetcher(NgxUrlAsyncFetcher *parent, char* proxy);

    ~NgxUrlAsyncFetcher();

    // It should be called in the module init_process callback function. Do some
    // intializations which can't be done in the master process
    bool Init(); 
    
    // shutdown all the fetches.
    virtual void ShutDown(); 

    virtual bool SupportsHttps() const { return false; }

    virtual void Fetch(const GoogleString& url,
                       MessageHandler* message_handler,
                       AsyncFetch* callback);

    // send the command from the current thread to main thread
    bool SendCmd(const char command);
    // the read handler in the main thread
    static void CommandHandler(ngx_event_t* cmdev);
    bool StartFetch(NgxFetch* fetch);

    // Remove the completed fetch from the active fetch set, and put it into a
    // completed fetch list to be cleaned up.
    void FetchComplete(NgxFetch* fetch);

    void PrintActiveFetches(MessageHandler* handler) const;
    
    // Indicates that it should track the original content length for
    // fetched resources.
    bool track_original_content_length() {
      return track_original_content_length_;
    }
    void set_track_original_content_length(bool x) {
      track_original_content_length_ = x;
    }
 
    typedef Pool<NgxFetch> NgxFetchPool;

    // AnyPendingFetches is accurate only at the time of call; this is
    // used conservatively during shutdown.  It counts fetches that have been
    // requested by some thread, and can include fetches for which no action
    // has yet been taken (ie fetches that are not active).
    virtual bool AnyPendingFetches() {
      return !active_fetches_.empty();
    }

    // ApproximateNumActiveFetches can under- or over-count and is used only for
    // error reporting.
    int ApproximateNumActiveFetches() {
      return active_fetches_.size();
    }

    void CancelActiveFetches();

    // These must be accessed with mutex_ held.
    bool shutdown() const { return shutdown_; }
    void set_shutdown(bool s) { shutdown_ = s; }


  private:
    static void TimeoutHandler(ngx_event_t* tev);
    friend class NgxFetch;

    NgxFetchPool active_fetches_;
    // Add the pending task to this list
    NgxFetchPool pending_fetches_;
    ngx_url_t url_;

    int fetchers_count_;
    bool shutdown_;
    bool track_original_content_length_;
    int64 byte_count_;
    ThreadSystem* thread_system_;
    MessageHandler* message_handler_;
    // Protect the member variable in this class
    // active_fetches, pending_fetches
    ThreadSystem::CondvarCapableMutex* mutex_;

    ngx_pool_t* pool_;
    ngx_log_t* log_;
    ngx_connection_t* command_connection_; // the command pipe
    int pipe_fd_; // the write pipe end
    ngx_resolver_t* resolver_;
    ngx_msec_t resolver_timeout_;
    ngx_msec_t fetch_timeout_;

    DISALLOW_COPY_AND_ASSIGN(NgxUrlAsyncFetcher);
};
} // net_instaweb

#endif
