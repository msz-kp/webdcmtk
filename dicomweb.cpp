#include <memory>
#include <ranges>
#include <sstream>
#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmdata/dcistrmb.h"
#include "dcmtk/dcmdata/dcsequen.h"
#include "drogon/HttpTypes.h"
#include "drogon/utils/Utilities.h"
#include "trantor/utils/Logger.h"

#include "dicomweb.h"
#include "dimse.h"
#include "fmt_extra.h"
#include "image.h"
#include "mime.h"
#include "misc.h"

bool isValidDicomTag(std::string s) {
  int n = s.length();
  if (n != 8)
    return false;

  for (int i = 0; i < n; i++) {
    char ch = s[i];

    if ((ch < '0' || ch > '9') &&
        (ch < 'A' || ch > 'F')) {
      return false;
    }
  }

  return true;
}

using namespace drogon;

HttpResponsePtr jsonErrResposne(HttpStatusCode statusCode, std::string message) {
  Json::Value errmsg;
  errmsg["status"] = "error";
  errmsg["code"] = statusCode;
  errmsg["message"] = message;
  // Json::Value errmsg is on stack value..
  auto errresp = HttpResponse::newHttpJsonResponse(errmsg);
  errresp->setStatusCode(statusCode);
  return errresp;
}

// DicomWebCFindToJson
DicomWebCFindToJson::DicomWebCFindToJson(const drogon::SafeStringMap<std::string> &requestParameters) : requestParameters{requestParameters} {}

void DicomWebCFindToJson::Perform(std::function<void(const HttpResponsePtr &)> &callback) {
  HttpResponsePtr response;

  parseParams();
  if (performCFind()) {
    response = HttpResponse::newHttpResponse();
    response->setCustomStatusCode(200);
    response->setContentTypeString("application/dicom+json");
    response->setBody(response_stream.str());
  } else
    response = jsonErrResposne(k503ServiceUnavailable, "DIMSE connection error");

  callback(response);
}

void DicomWebCFindToJson::parseParams() {
  offset = 0;
  limit = 0;

  for (auto &p : requestParameters) {
    if (stringCaseCmp(p.first, "limit")) {
      limit = std::stoi(p.second);
    } else if (stringCaseCmp(p.first, "offset")) {
      offset = std::stoi(p.second);
    } else if (stringCaseCmp(p.first, "includefield")) {
      for (const auto &t : std::views::split(p.second, ','))
        includefield.insert(std::string(t.begin(), t.end()));
    } else if (isValidDicomTag(p.first)) {
      queryTags[p.first] = p.second;
    } else {
      auto tag = TagNamesTranslations.find(p.first);
      if (tag != TagNamesTranslations.end())
        queryTags[(*tag).second] = p.second;
      else
        LOG_INFO << "incorrect param: '" << p.first << "'";
    }
  }
}

bool DicomWebCFindToJson::performCFind() {
  for (auto &f : includefield) {
    if (!queryTags.contains(f))
      queryTags[f] = "";
  }
  DcmDataset query;
  query.putAndInsertString(DCM_QueryRetrieveLevel, level.c_str());

  for (auto &q : queryTags) {
    int htag = stoi(q.first, 0, 16);
    Uint16 group = htag >> 16;
    Uint16 elem = htag & 0x0000ffff;
    if (q.second.empty())
      query.putAndInsertString(DcmTagKey(group, elem), NULL);
    else
      query.putAndInsertOFStringArray(DcmTagKey(group, elem), q.second);
  }

  EasyDcmFind scu;
  auto res = scu.connect();
  if (res.bad()) {
    LOG_ERROR << "performCFind(): " << res.text();
    return false;
  }
  res = scu.find(query, limit + offset);
  if (res.bad()) {
    LOG_ERROR << "performCFind(): " << res.text();
    return false;
  }

  CustomJsonFormat jsonfmt;
  response_stream << "[\n";

  auto iterBegin = scu.response.begin();
  std::advance(iterBegin, offset);

  for (auto iter = iterBegin; iter != scu.response.end(); ++iter) {
    (*iter)->writeJsonExt(response_stream, jsonfmt, true, false);

    if (std::next(iter) != scu.response.end())
      response_stream << ",\n";
  }
  response_stream << "\n]\n";

  scu.disconnect();
  if (res.bad()) {
    LOG_WARN << "performCFind(): " << res.text();
  }

  return true;
}

// DicomWebCGetToResposne
DicomWebCGetToResposne::DicomWebCGetToResposne(const drogon::SafeStringMap<std::string> &requestParameters) {
  offset = 0;
  limit = 0;

  for (auto &p : requestParameters) {
    if (stringCaseCmp(p.first, "limit")) {
      LOG_WARN << "limit not yet implementd. beware!";
      limit = std::stoi(p.second);
    } else if (stringCaseCmp(p.first, "offset")) {
      LOG_WARN << "offset not yet implementd. beware!";
      offset = std::stoi(p.second);
    } else if (isValidDicomTag(p.first)) {
      queryTags[p.first] = p.second;
    } else {
      auto tag = TagNamesTranslations.find(p.first);
      if (tag != TagNamesTranslations.end())
        queryTags[(*tag).second] = p.second;
      else
        LOG_INFO << "incorrect param: '" << p.first << "'";
    }
  }
}

void DicomWebCGetToResposne::PerformOne(std::function<void(const drogon::HttpResponsePtr &)> &callback, headerAccept &haccept, int frameNo, unsigned long resizeWidth, unsigned long resizeHeight) {
  HttpResponsePtr response;
  unsigned long size = 0;
  char *rBuff = NULL;
  std::shared_ptr<void> frameBuff;
  std::shared_ptr<DcmFileFormatBuffer> dcmBuff;

  if (performCGet()) {

    if (scu.response.size() == 1) {
      auto dcm = scu.response.front();

      if (haccept.type == "application/octet-stream") {
        frameBuff = getFrame(dcm, frameNo - 1, &size);
        if (frameBuff) {
          rBuff = reinterpret_cast<char *>(frameBuff.get());
        } else {
          callback(jsonErrResposne(drogon::k500InternalServerError, "frame extraction error"));
          return;
        }
      } else if (haccept.type == "application/dicom") {
	// let drogon handle exception for DcmFileFormatBuffer constructor - no try here
	dcmBuff = std::make_shared<DcmFileFormatBuffer>(dcm);
	rBuff = reinterpret_cast<char *>(dcmBuff->buffer);
	size = dcmBuff->size;
      }
      else if (haccept.type == "image/jpeg") {
	if (!getJpeg(dcm, &rBuff, &size, frameNo, resizeWidth, resizeHeight)) {
          callback(jsonErrResposne(drogon::k500InternalServerError, "jpeg conversion error"));
          return;
	}
      } else {
	callback(jsonErrResposne(drogon::k406NotAcceptable, "unsupported Accept: " + haccept.type));
	return;
      }

    } else if (scu.response.size() == 0) {
      callback(response = jsonErrResposne(k404NotFound, "not found"));
      return;
    } else {
      callback(response = jsonErrResposne(k400BadRequest, "found instances number > 1"));
      return;
    }

  } else {
    callback(response = jsonErrResposne(k503ServiceUnavailable, "DIMSE connection error"));
    return;
  }

  std::stringstream response_stream;

  if (haccept.multipart_related) {
    std::string boundary = drogon::utils::secureRandomString(32);
    response_stream << "--" << boundary << "\r\n"
                    << "Content-Type: " << haccept.type << "\r\n"
                    << "Content-Length: " << size << "\r\n"
                    << "MIME-Version: 1.0\r\n"
                    << "\r\n";
    response_stream.write(rBuff, size);
    response_stream << "\r\n";
    response_stream << "--" << boundary << "--" << "\r\n";

    response = HttpResponse::newHttpResponse();
    response->setBody(response_stream.str());
    response->setCustomStatusCode(200);
    response->setContentTypeString("multipart/related; type=\"" + haccept.type + "\"; boundary=" + boundary);
  } else {
    response_stream.write(rBuff, size);

    response = HttpResponse::newHttpResponse();
    response->setBody(response_stream.str());
    response->setCustomStatusCode(200);
    response->setContentTypeString(haccept.type);
  }

  callback(response);
}

void DicomWebCGetToResposne::Perform(std::function<void(const HttpResponsePtr &)> &callback) {
  HttpResponsePtr response;
  std::string boundary = drogon::utils::secureRandomString(32);
  std::string c_type = "multipart/related; type=\"application/dicom\"; boundary=";
  c_type.append(boundary);

  if (performCGet()) {
    if (scu.response.size() != 0) {
      std::stringstream response_stream;
      for (auto &dcm : scu.response) {
        // let drogon handle exception for DcmFileFormatBuffer constructor - no try here
        DcmFileFormatBuffer buff(dcm);
        char *rBuff = reinterpret_cast<char *>(buff.buffer);
        response_stream << "--" << boundary << "\r\n"
                        << "Content-Type: application/dicom" << "\r\n"
                        << "Content-Length: " << buff.size << "\r\n"
                        << "MIME-Version: 1.0\r\n"
                        << "\r\n";
        response_stream.write(rBuff, buff.size);
        response_stream << "\r\n";
      }
      response_stream << "--" << boundary << "--" << "\r\n";

      response = HttpResponse::newHttpResponse();
      response->setBody(response_stream.str());
      response->setCustomStatusCode(200);
      response->setContentTypeString(c_type);
    } else
      response = jsonErrResposne(k404NotFound, "not found");
  } else
    response = jsonErrResposne(k503ServiceUnavailable, "DIMSE connection error");

  callback(response);
}

bool DicomWebCGetToResposne::performCGet() {
  DcmDataset query;
  for (auto &q : queryTags) {
    int htag = stoi(q.first, 0, 16);
    Uint16 group = htag >> 16;
    Uint16 elem = htag & 0x0000ffff;
    query.putAndInsertOFStringArray(DcmTagKey(group, elem), q.second);
  }

  auto res = scu.connect();
  if (res.bad()) {
    LOG_ERROR << "performCGet(): " << res.text();
    return false;
  }
  res = scu.get(query);
  if (res.bad()) {
    LOG_ERROR << "performCGet(): " << res.text();
    return false;
  }
  scu.disconnect();

  return true;
}

// STOW
STOWMimeParser::STOWMimeParser(std::shared_ptr<EasyDcmStore> scu, const std::string &boundary) : Consumer(boundary), scu{scu} {}
STOWMimeParser::~STOWMimeParser() {};

void STOWMimeParser::receiveHeader(const std::string &name, const std::string &value) {
  if (name != "Content-Type")
    return;

  if (value == "application/dicom")
    shouldAdd = true;
  else {
    LOG_WARN << "STOW unsupported Content-Type: " << value;
    shouldAdd = false;
  }
}

void STOWMimeParser::receiveData(const char *at, size_t length) {
  stream.write(at, length);
}

void STOWMimeParser::receiveDataEnd() {
  if (!shouldAdd) {
    LOG_WARN << "STOW data ignored. Unsupported Content-Type ?";
    return;
  }

  DcmInputBufferStream dcmStream;
  std::string buffer = stream.str();
  dcmStream.setBuffer(buffer.data(), buffer.size());
  dcmStream.setEos();

  std::shared_ptr<DcmFileFormat> ff = std::make_shared<DcmFileFormat>();
  OFCondition res = ff->read(dcmStream);
  if (res.good()) {
    res = scu->addFile(ff);
    if (res.bad())
      LOG_ERROR << "CSTORE addFile() failed: " << res.text();
  } else
    LOG_ERROR << "CSTORE invalid data: " << res.text();

  dcmStream.releaseBuffer();
  stream = std::stringstream();
}

void DicomStowResposne::Perform(std::function<void(const drogon::HttpResponsePtr &)> &callback, std::string boundary, const char *body, size_t bodyLen) {
  std::shared_ptr<EasyDcmStore> scu = std::make_shared<EasyDcmStore>();
  STOWMimeParser parser(scu, boundary);

  parser.decode(body, bodyLen);

  auto status = scu->connect();
  if (status.bad()) {
    callback(jsonErrResposne(k503ServiceUnavailable, (std::string("DIMSE connection error: ") + status.text())));
    return;
  }

  scu->send();

  DcmSequenceOfItems *seqOK = new DcmSequenceOfItems(DcmTag(0x0008, 0x1199));
  DcmSequenceOfItems *seqFAIL = new DcmSequenceOfItems(DcmTag(0x0008, 0x1198));
  for (auto &dcm : scu->getFiles()) {
    DcmItem *resDcm = new DcmItem;
    resDcm->putAndInsertOFStringArray(DcmTag(0x0008, 0x1155), dcm.sopInstanceUID);
    resDcm->putAndInsertOFStringArray(DcmTag(0x0008, 0x1150), dcm.mediaStorageSOPClassUID);

    if (dcm.sent)
      seqOK->append(resDcm);
    else {
      // no json output ?
      // resDcm->putAndInsertOFStringArray(DCM_FailureReason, dcm.error);
      seqFAIL->append(resDcm);
    }
  }
  DcmDataset ds;
  ds.insert(seqOK);
  ds.insert(seqFAIL);

  CustomJsonFormat jsonfmt;
  std::stringstream response_stream;
  ds.writeJsonExt(response_stream, jsonfmt, true, false);

  HttpResponsePtr response;
  response = HttpResponse::newHttpResponse();
  response->setCustomStatusCode(200);
  response->setContentTypeString("application/dicom+json");
  response->setBody(response_stream.str());
  callback(response);
}
