#include "PsychicJson.h"

PsychicJsonResponse::PsychicJsonResponse(PsychicResponse* response, bool isArray)
    : PsychicResponseDelegate(response)
{
    setContentType(JSON_MIMETYPE);
    _jsonBuffer.s = isArray ? "[]" : "{}";
}

size_t PsychicJsonResponse::getLength()
{
    return _jsonBuffer.s.length();
}

esp_err_t PsychicJsonResponse::send()
{
    esp_err_t    err         = ESP_OK;
    const size_t length      = getLength();
    const size_t buffer_size = (length < JSON_BUFFER_SIZE) ? (length + 1) : JSON_BUFFER_SIZE;

    char* buffer = static_cast<char*>(malloc(buffer_size));
    if (!buffer)
        return error(HTTPD_500_INTERNAL_SERVER_ERROR, "Unable to allocate memory.");

    if (length < JSON_BUFFER_SIZE) {
        memcpy(buffer, _jsonBuffer.s.c_str(), length);
        buffer[length] = '\0';
        setContent(reinterpret_cast<uint8_t*>(buffer), length);
        setContentType(JSON_MIMETYPE);
        err = PsychicResponseDelegate::send();
    } else {
        ChunkPrinter dest(_response, reinterpret_cast<uint8_t*>(buffer), buffer_size);
        sendHeaders();

        const char* data = _jsonBuffer.s.c_str();
        size_t      sent = 0;

        while (sent < length) {
            const size_t chunk = std::min(buffer_size - 1, length - sent);
            memcpy(buffer, data + sent, chunk);
            buffer[chunk] = '\0';

            if (dest.write(reinterpret_cast<uint8_t*>(buffer), chunk) != chunk) {
                err = ESP_FAIL;
                break;
            }
            sent += chunk;
        }

        dest.flush();
        err = finishChunking();
    }

    free(buffer);
    return err;
}

PsychicJsonHandler::PsychicJsonHandler(size_t maxContentLength)
    : _onRequest(nullptr), _maxContentLength(maxContentLength),
      _bodyBuffer(nullptr), _bodyBufferSize(0) {}

PsychicJsonHandler::PsychicJsonHandler(PsychicJsonRequestCallback onRequest,
                                       size_t maxContentLength)
    : _onRequest(std::move(onRequest)), _maxContentLength(maxContentLength),
      _bodyBuffer(nullptr), _bodyBufferSize(0) {}

PsychicJsonHandler::~PsychicJsonHandler()
{
    cleanupBuffer();
}

void PsychicJsonHandler::cleanupBuffer() noexcept
{
    if (_bodyBuffer) {
        free(_bodyBuffer);
        _bodyBuffer     = nullptr;
        _bodyBufferSize = 0;
    }
}

void PsychicJsonHandler::onRequest(PsychicJsonRequestCallback fn)
{
    _onRequest = std::move(fn);
}
esp_err_t PsychicJsonHandler::handleRequest(PsychicRequest* request, PsychicResponse* response)
{
    PsychicWebHandler::handleRequest(request, response);

    if (!_onRequest)
        return response->send(500, "text/plain", "No handler configured");

    // canHandle() already enforced the method mask, so any request that
    // reaches here has an allowed method.  Decide whether to expect a body.
    const http_method requestMethod = request->method();
    const bool hasBody = (requestMethod == HTTP_POST  ||
                          requestMethod == HTTP_PUT   ||
                          requestMethod == HTTP_PATCH ||
                          requestMethod == HTTP_DELETE);

    if (hasBody) {
        const size_t contentLen = request->contentLength();

        if (contentLen > _maxContentLength)
            return response->send(413, "text/plain", "Content too large");
        if (contentLen == 0)
            return response->send(400, "text/plain", "Empty request body");

        const String contentType = request->contentType();
        if (!contentType.equalsIgnoreCase(JSON_MIMETYPE))
            return response->send(400, "text/plain", "Content-Type must be application/json");

        _bodyBuffer = static_cast<uint8_t*>(malloc(contentLen + 1));
        if (!_bodyBuffer)
            return response->send(500, "text/plain", "Out of memory");

        const String body = request->body();
        int received = 0;

        if (body.length() == contentLen) {
            memcpy(_bodyBuffer, body.c_str(), contentLen);
            received = contentLen;
        } else {
            httpd_req_t* req = request->request();
            received = httpd_req_recv(req, reinterpret_cast<char*>(_bodyBuffer), contentLen);
            if (received <= 0) {
                cleanupBuffer();
                return response->send(400, "text/plain", "Failed to read request body");
            }
        }

        _bodyBuffer[received] = '\0';
        _bodyBufferSize = received;

        gson::Parser parser;
        if (!parser.parse(reinterpret_cast<char*>(_bodyBuffer), _bodyBufferSize) || parser.hasError()) {
            cleanupBuffer();
            return response->send(400, "text/plain", "Invalid JSON");
        }

        esp_err_t result = _onRequest(request, response, parser);
        cleanupBuffer();
        return result;
    }

    // Bodyless method (GET, HEAD, etc.) — pass an empty parser
    gson::Parser parser;
    return _onRequest(request, response, parser);
}
