#include "drogon/HttpController.h"
#include "drogon/HttpTypes.h"

#include "dicomweb.h"
#include "fmt_extra.h"
#include "image.h"
#include "misc.h"

using namespace drogon;

typedef enum {
  responseSTUDY,
  responseSERIES,
  responseIMAGE,
  responseMETADATA,
  responseUNDEFINED
} acceptBasedResposneType;

void acceptBasedResposne(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback, acceptBasedResposneType type,
                         std::string *studyInstanceUID, std::string *seriesInstanceUID, std::string *SOPInstanceUID,
                         std::string default_accept = "application/dicom+json") {
  headerAccept haccept;

  // FIXME: temp sollution
  if (req->getHeader("Accept") == "*/*")
    haccept.type = default_accept;
  else
    haccept = defaultHeaderAccept(req->getHeader("Accept"));


  if (haccept.type == "application/dicom") {
    if (studyInstanceUID == NULL) {
      callback(jsonErrResposne(k422UnprocessableEntity, "studyInstanceUID required"));
      return;
    }

    if (!(haccept.transfer_syntax.empty() || haccept.transfer_syntax == "*")) {
      callback(jsonErrResposne(k501NotImplemented, "user specified transferSyntax unsupported"));
      return;
    }

    auto handler = DicomWebCGetToResposne(req->getParameters());

    if (studyInstanceUID)
      handler.queryTags["0020000d"] = *studyInstanceUID;
    if (seriesInstanceUID)
      handler.queryTags["0020000e"] = *seriesInstanceUID;
    if (SOPInstanceUID)
      handler.queryTags["00080018"] = *SOPInstanceUID;

    handler.Perform(callback);
  } else if (haccept.type == "application/dicom+json") {
    auto handler = DicomWebCFindToJson(req->getParameters());

    if (type == responseSTUDY) {
      handler.level = "STUDY";
      handler.includefield = STUDYReturnedAttributes;
    } else if (type == responseSERIES) {
      handler.level = "SERIES";
      handler.includefield = SERIESReturnedAttributes;
    } else if (type == responseIMAGE) {
      handler.level = "IMAGE";
      handler.includefield = INSTANCEReturnedAttributes;
    } else if (type == responseMETADATA) {
      handler.level = "IMAGE";
      handler.includefield = METADATAReturnedAttributes;
    }

    if (studyInstanceUID)
      handler.queryTags["0020000d"] = *studyInstanceUID;
    if (seriesInstanceUID)
      handler.queryTags["0020000e"] = *seriesInstanceUID;
    if (SOPInstanceUID)
      handler.queryTags["00080018"] = *SOPInstanceUID;

    handler.Perform(callback);
  } else {
    callback(jsonErrResposne(k406NotAcceptable, "unsupported Accept: " + haccept.type));
  }
}

class DicomWeb : public HttpController<DicomWeb> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(DicomWeb::studies, "/dicomweb/studies", Get);
  ADD_METHOD_TO(DicomWeb::series, "/dicomweb/series", Get);
  ADD_METHOD_TO(DicomWeb::unimplemented, "/dicomweb/instances", Get); // crazzy !
  ADD_METHOD_TO(DicomWeb::unimplemented, "/dicomweb/frames", Get);

  ADD_METHOD_TO(DicomWeb::study_series, "/dicomweb/studies/{studyInstanceUID}/series", Get);

  ADD_METHOD_TO(DicomWeb::study_instances, "/dicomweb/studies/{StudyInstanceUID}/instances", Get);
  ADD_METHOD_TO(DicomWeb::serie_instances, "/dicomweb/studies/{studyInstanceUID}/series/{seriesInstanceUID}/instances", Get);
  ADD_METHOD_TO(DicomWeb::instance, "/dicomweb/studies/{StudyInstanceUID}/series/{seriesInstanceUID}/instances/{SOPInstanceUID}", Get);

  ADD_METHOD_TO(DicomWeb::instance_frames, "/dicomweb/studies/{StudyInstanceUID}/series/{seriesInstanceUID}/instances/{SOPInstanceUID}/frames/{frame_no}", Get);

  ADD_METHOD_TO(DicomWeb::study_metadata, "/dicomweb/studies/{studyInstanceUID}/metadata", Get); // wtf. dgate cfind return to much. unlucky combination of returned tags or.. bug ?!
  ADD_METHOD_TO(DicomWeb::serie_metadata, "/dicomweb/studies/{studyInstanceUID}/series/{seriesInstanceUID}/metadata", Get);
  ADD_METHOD_TO(DicomWeb::instance_metadata, "/dicomweb/studies/{StudyInstanceUID}/series/{seriesInstanceUID}/instances/{SOPInstanceUID}/metadata", Get);

  ADD_METHOD_TO(DicomWeb::wado, "/wado", Get);

  ADD_METHOD_TO(DicomWeb::stow, "/dicomweb/studies", Post);

  METHOD_LIST_END

protected:
  void unimplemented(const HttpRequestPtr &, std::function<void(const HttpResponsePtr &)> &&callback) {
    callback(jsonErrResposne(k501NotImplemented, "unimplemented"));
  }

  void studies(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
    acceptBasedResposne(req, std::move(callback), responseSTUDY, NULL, NULL, NULL);
  }

  void study_series(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback, std::string &&studyInstanceUID) {
    acceptBasedResposne(req, std::move(callback), responseSERIES, &studyInstanceUID, NULL, NULL);
  }

  void series(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
    acceptBasedResposne(req, std::move(callback), responseSERIES, NULL, NULL, NULL);
  }

  void serie_instances(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback, std::string &&studyInstanceUID, std::string &&seriesInstanceUID) {
    acceptBasedResposne(req, std::move(callback), responseIMAGE, &studyInstanceUID, &seriesInstanceUID, NULL);
  }

  void study_instances(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback, std::string &&studyInstanceUID) {
    acceptBasedResposne(req, std::move(callback), responseIMAGE, &studyInstanceUID, NULL, NULL);
  }

  void instance_metadata(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback, std::string &&studyInstanceUID, std::string &&seriesInstanceUID, std::string &&SOPInstanceUID) {
    acceptBasedResposne(req, std::move(callback), responseMETADATA, &studyInstanceUID, &seriesInstanceUID, &SOPInstanceUID);
  }

  void serie_metadata(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback, std::string &&studyInstanceUID, std::string &&seriesInstanceUID) {
    acceptBasedResposne(req, std::move(callback), responseMETADATA, &studyInstanceUID, &seriesInstanceUID, NULL);
  }

  void study_metadata(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback, std::string &&studyInstanceUID) {
    acceptBasedResposne(req, std::move(callback), responseMETADATA, &studyInstanceUID, NULL, NULL);
  }

  void instance(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback, std::string &&studyInstanceUID, std::string &&seriesInstanceUID, std::string &&SOPInstanceUID) {
    //acceptBasedResposne(req, std::move(callback), responseIMAGE, &studyInstanceUID, &seriesInstanceUID, &SOPInstanceUID, "application/dicom");

    auto handler = DicomWebCGetToResposne(req->getParameters());

    handler.queryTags["0020000d"] = studyInstanceUID;
    handler.queryTags["0020000e"] = seriesInstanceUID;
    handler.queryTags["00080018"] = SOPInstanceUID;

    auto haccept = defaultHeaderAccept(req->getHeader("Accept"));
    handler.PerformOne(callback, haccept);
  }

  void instance_frames(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback, std::string &&studyInstanceUID, std::string &&seriesInstanceUID, std::string &&SOPInstanceUID, int frameNo) {
    auto handler = DicomWebCGetToResposne(req->getParameters());

    handler.queryTags["0020000d"] = studyInstanceUID;
    handler.queryTags["0020000e"] = seriesInstanceUID;
    handler.queryTags["00080018"] = SOPInstanceUID;

    auto haccept = defaultHeaderAccept(req->getHeader("Accept"));
    handler.PerformOne(callback, haccept, frameNo);
  }

  void wado(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
    std::unordered_map<std::string, std::string> queryTags;

    if (req->getParameter("objectUID").empty() ||
        req->getParameter("seriesUID").empty() ||
        req->getParameter("objectUID").empty() ||
        req->getParameter("requestType") != "WADO") {
      callback(jsonErrResposne(k400BadRequest, "bad params"));
      return;
    }

    if (!(req->getParameter("transferSyntax").empty() || req->getParameter("transferSyntax") == "*")) {
      callback(jsonErrResposne(k501NotImplemented, "user specified transferSyntax unsupported"));
      return;
    }

    if (!req->getParameter("charset​").empty() ||
        //!req->getParameter("frameNumber​").empty() ||
        !req->getParameter("imageAnnotation​").empty() ||
        !req->getParameter("imageQuality​").empty() ||
        //!req->getParameter("rows​").empty() ||
        //!req->getParameter("columns​").empty() ||
        !req->getParameter("region​").empty() ||
        !req->getParameter("windowCenter​").empty() ||
        !req->getParameter("windowWidth​").empty() ||
        !req->getParameter("presentationSeriesUID​").empty() ||
        !req->getParameter("presentationUID​").empty()) {
      callback(jsonErrResposne(k501NotImplemented, "unsupported parameters"));
      return;
    }

    drogon::SafeStringMap<std::string> empty;
    auto handler = DicomWebCGetToResposne(empty);
    handler.queryTags["0020000d"] = req->getParameter("studyUID");
    handler.queryTags["0020000e"] = req->getParameter("seriesUID");
    handler.queryTags["00080018"] = req->getParameter("objectUID");


    headerAccept haccept;
    haccept.transfer_syntax = req->getParameter("transferSyntax");
    haccept.type = req->getParameter("contentType").empty() ? "application/dicom" : req->getParameter("contentType");

    int imgH = req->getParameter("rows").empty() ? 0 : stoi(req->getParameter("rows"));
    int imgW = req->getParameter("columns").empty() ? 0 : stoi(req->getParameter("columns"));
    int frameNo = req->getParameter("frameNumber").empty() ? 1 : stoi(req->getParameter("frameNumber"));

    handler.PerformOne(callback, haccept, frameNo, imgW, imgH);
  }

  void stow(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
    headerAccept haccept = defaultHeaderAccept(req->getHeader("Accept"));
    if (!(haccept.type == "application/dicom+json" || haccept.type == "*/*")) {
      callback(jsonErrResposne(k406NotAcceptable, "unsupported Accept: " + haccept.type));
      return;
    }
    const char *body = req->bodyData();
    size_t bodyLen = req->bodyLength();
    // always ?
    if (body[0] == '\r' && body[1] == '\n') {
      body = body + 2;
      bodyLen = bodyLen - 2;
    }

    auto handler = DicomStowResposne();
    auto reqCT = parseHeaderAccept(req->getHeader("content-type"));
    handler.Perform(callback, reqCT.boundary, body, bodyLen);
  }
};

static HttpResponsePtr customErrorHandler(HttpStatusCode statusCode, const HttpRequestPtr &) {
  Json::Value errmsg;
  errmsg["status"] = "error";
  errmsg["code"] = statusCode;
  errmsg["message"] = "Unexpected server error";

  auto errresp = HttpResponse::newHttpJsonResponse(errmsg);
  errresp->setStatusCode(statusCode);
  return errresp;
}

static const HttpResponsePtr custom404() {
  Json::Value msg404;
  msg404["code"] = 404;
  msg404["message"] = "not found";
  return HttpResponse::newHttpJsonResponse(msg404);
}

int main() {
  OFLog::configure(OFLogger::FATAL_LOG_LEVEL);
  LOG_COMPACT_INFO << "starting..";
  app().loadConfigFile("./config.json");
  registerCodecs();
  app().setCustomErrorHandler(customErrorHandler);
  auto res404 = custom404();
  app().setCustom404Page(res404);
  app().run();
  return 0;
}
