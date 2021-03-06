/* Copyright (c) 2019 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License,
 * attached with Common Clause Condition 1.0, found in the LICENSES directory.
 */

#include "http/HttpClient.h"
#include "process/ProcessUtils.h"

#include <curl/curl.h>

static struct __curl_init_helper {
    __curl_init_helper() {
        curl_global_init(CURL_GLOBAL_ALL);
    }
    ~__curl_init_helper() {
        curl_global_cleanup();
    }
}curl_init_helper;

struct CxxCurl {
    CxxCurl() {}

    ~CxxCurl() {
        if (curl) {
            curl_easy_cleanup(curl);
        }
    }

    bool isValid()const {
        return curl != nullptr;
    }

    std::string&& getResponse(){
        return std::move(response);
    }

    const std::string& getResponse()const{
        return response;
    }

    CURLcode get(const std::string& path) {
        curl_easy_setopt(curl, CURLOPT_URL, path.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, response_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        return curl_easy_perform(curl);
    }

    CURLcode post(const std::string& path, const std::vector<std::string>* headers,
        const void* data, size_t dataLength) {
        curl_easy_setopt(curl, CURLOPT_URL, path.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, response_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_slist* list = nullptr;
        if (headers != nullptr && !headers->empty()) {
            for (auto headerIter = headers->cbegin();
                headerIter != headers->cend(); ++headerIter) {
                list = curl_slist_append(list, headerIter->c_str());
            }
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
        }
        if (data != nullptr && dataLength != 0) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, dataLength);
        }
        CURLcode retCode = curl_easy_perform(curl);
        if (list != nullptr) {
            curl_slist_free_all(list);
        }
        return retCode;
    }

private:
    static size_t response_callback(char* data, size_t size, size_t nmemb, void* userptr);
    CURL* curl = curl_easy_init();
    std::string response;
};

size_t CxxCurl::response_callback(char* data, size_t size, size_t nmemb, void* userptr) {
    std::string* response = reinterpret_cast<std::string*>(userptr);
    size *= nmemb;
    response->append(data, size);
    return size;
}

namespace nebula {
namespace http {

StatusOr<std::string> HttpClient::get(const std::string& path, const std::string& options) {
    (void)options;
    CxxCurl curl;
    if (!curl.isValid()) {
        return Status::Error("curl_easy_init() failed");
    }
    CURLcode res = curl.get(path);
    if (res != CURLE_OK) {
        return Status::Error("curl_easy_perform() failed: %s", curl_easy_strerror(res));
    }
    return curl.getResponse();
}

StatusOr<std::string> HttpClient::post(const std::string& path,
    const std::vector<std::string>* headers, const void* data, size_t dataLength) {
    CxxCurl curl;
    if (!curl.isValid()) {
        return Status::Error("curl_easy_init() failed");
    }
    CURLcode res = curl.post(path, headers, data, dataLength);
    if (res != CURLE_OK) {
        return Status::Error("curl_easy_perform() failed: %s", curl_easy_strerror(res));
    }
    return curl.getResponse();
}

}   // namespace http
}   // namespace nebula
