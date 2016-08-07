#include <stdio.h>
#include <string.h>
#include <curl/curl.h>

#include "apr_strings.h"
#include "types.h"
#include "http_util.h"

#define JSON_CONTENT_TYPE "Content-Type: application/json"
#define EXPECT_HEADER "Expect:"

const char *RISK_API_URL = "http://collector.perimeterx.net/api/v1/risk";
const char *CAPTHCA_API_URL = "http://collector.perimeterx.net/api/v1/risk/captcha";
const char *ACTIVITIES_URL = "http://collector.perimeterx.net/api/v1/collector/s2s";

char *do_request(const char *url, char *payload, char *auth_header, request_rec *r, CURL *curl);

static const char *s2s_call_reason_string(s2s_call_reason_t r) {
    static const char *call_reasons[] = { "none", "no_cookie", "expired_cookie", "invalid_cookie"};
    return call_reasons[r];
}

static const char *block_reason_string(block_reason_t r) {
    static const char *block_reason[] = { "none", "cookie_high_score", "s2s_high_score" };
    return block_reason[r];
}

static size_t write_response_cb(void* contents, size_t size, size_t nmemb, void *stream) {
    size_t realsize = size * nmemb;
    struct response_t *res = (struct response_t*)stream;
    res->data = realloc(res->data, res->size + realsize + 1);
    if (res->data == NULL) {
        printf("Not enough memory (realloc returned NULL)\n");
        return 0;
    }
    memcpy(&(res->data[res->size]), contents, realsize);
    res->size += realsize;
    res->data[res->size] = 0;

    return realsize;
}

char* risk_api_request(char *risk_payload, char *auth_header, request_rec *r, CURL *curl) {
    return do_request(RISK_API_URL, risk_payload, auth_header, r, curl);
}

int send_activity(char* activity, char* auth_header, request_rec *r, CURL *curl) {
    return do_request(ACTIVITIES_URL, activity, auth_header, r, curl) != NULL ? REQ_SUCCESS : REQ_FAILED;
}

char* captcha_validation_request(char *captcha_payload, char *auth_header,  request_rec *r, CURL *curl) {
    return do_request(CAPTHCA_API_URL ,captcha_payload, auth_header, r, curl);
}

// General function to make http request to px servers
char *do_request(const char *url, char *payload, char *auth_header, request_rec *r, CURL *curl) {
    struct response_t response;
    struct curl_slist *headers = NULL;
    long status_code;
    CURLcode res;

    response.data = malloc(1);
    response.size = 0;

    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, JSON_CONTENT_TYPE);
    headers = curl_slist_append(headers, EXPECT_HEADER);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*) &response);
    res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
        if (status_code  == 200) {
            return response.data;
        }
        free(response.data);
        ERROR(r->server, "PX server request returned status: %ld, body: %s", status_code, response.data);
    } else {
        ERROR(r->server, "curl_easy_perform() failed: %s", curl_easy_strerror(res));
    }
    return NULL;
}
