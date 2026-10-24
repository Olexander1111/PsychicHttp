
#include "ChunkPrinter.h"

ChunkPrinter::ChunkPrinter(PsychicResponse* response, uint8_t* buffer, size_t len) : _response(response),
                                                                                     _buffer(buffer),
                                                                                     _length(len),
                                                                                     _pos(0),
                                                                                     _failed(false)
{
}

ChunkPrinter::~ChunkPrinter()
{
  flush();
}

size_t ChunkPrinter::write(uint8_t c)
{
  // Client gone: drop the rest instead of one failing send (and log line) per chunk.
  if (_failed)
    return 0;

  // if we're full, send a chunk
  if (_pos == _length)
  {
    _pos = 0;

  if (_response->sendChunk(_buffer, _length) != ESP_OK)
    {
      _failed = true;
       return 0;
    }
  }

  _buffer[_pos] = c;
  _pos++;
  return 1;
}

size_t ChunkPrinter::write(const uint8_t* buffer, size_t size)
{
  if (_failed)
    return 0;
  
  size_t written = 0;

  while (written < size)
  {
    size_t space = _length - _pos;
    size_t blockSize = std::min(space, size - written);

    memcpy(_buffer + _pos, buffer + written, blockSize);
    _pos += blockSize;

    if (_pos == _length)
    {
      _pos = 0;

      if (_response->sendChunk(_buffer, _length) != ESP_OK)
      {
        _failed = true;
         return written;
      }
    }
    written += blockSize; // Update if sent correctly.
  }
  return written;
}

void ChunkPrinter::flush()
{
  if (_pos && !_failed)
   {
    if (_response->sendChunk(_buffer, _pos) != ESP_OK)
      _failed = true;
   }
  _pos = 0;
}

size_t ChunkPrinter::copyFrom(Stream& stream)
{
  size_t count = 0;

  while (!_failed && stream.available())
  {

    if (_pos == _length)
    {
      _pos = 0;
        if (_response->sendChunk(_buffer, _length) != ESP_OK)
      {
        _failed = true;
        break;
      }
    }

    size_t readBytes = stream.readBytes(_buffer + _pos, _length - _pos);
    _pos += readBytes;
    count += readBytes;
  }
  return count;
}
