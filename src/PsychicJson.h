// PsychicJson.h
/*
  Async Response to use with GSON
*/
#ifndef PSYCHIC_JSON_H_
#define PSYCHIC_JSON_H_

#include "ChunkPrinter.h"
#include "PsychicRequest.h"
#include "PsychicWebHandler.h"
#include <GSON.h>
#include <cstring>
#include <functional>

constexpr const char* JSON_MIMETYPE = "application/json";

#ifndef JSON_BUFFER_SIZE
  #define JSON_BUFFER_SIZE (4 * 1024)
#endif

#ifndef MAX_JSON_CONTENT_LENGTH
  #define MAX_JSON_CONTENT_LENGTH 16384
#endif

class PsychicRequest;
class PsychicResponse;

using PsychicJsonRequestCallback =
    std::function<esp_err_t(PsychicRequest*, PsychicResponse*, gson::Parser&)>;

class PsychicJsonResponse : public PsychicResponseDelegate
{
protected:
    gson::string _jsonBuffer;
    bool _isValid = false;

public:
    PsychicJsonResponse(PsychicResponse* response, bool isArray = false);
    ~PsychicJsonResponse() = default;

    gson::string& getRoot() { return _jsonBuffer; }
    const gson::string& getRoot() const { return _jsonBuffer; }

    size_t    getLength();
    size_t    getSize() const { return _jsonBuffer.s.length(); }
    bool      isValid() const { return _isValid; }

    esp_err_t send();
};

class PsychicJsonHandler : public PsychicWebHandler
{
protected:
    PsychicJsonRequestCallback _onRequest;
    size_t   _maxContentLength;
    
    int _method = HTTP_POST | HTTP_PUT | HTTP_PATCH;

    uint8_t* _bodyBuffer;
    size_t   _bodyBufferSize;

public:
    explicit PsychicJsonHandler(size_t maxContentLength = MAX_JSON_CONTENT_LENGTH);
    PsychicJsonHandler(PsychicJsonRequestCallback onRequest,
                       size_t maxContentLength = MAX_JSON_CONTENT_LENGTH);
    ~PsychicJsonHandler();

    void onRequest(PsychicJsonRequestCallback fn);
    void setMaxContentLength(size_t maxContentLength) { _maxContentLength = maxContentLength; }
    void setMethod(int method) { _method = method; }

    esp_err_t handleRequest(PsychicRequest* request, PsychicResponse* response) override;

    bool isRequestHandlerTrivial() { return !_onRequest; }

private:
    void cleanupBuffer() noexcept;
};

#endif // PSYCHIC_JSON_H_
