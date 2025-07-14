#include "dcmtk/dcmimgle/dcmimage.h"
#include "dcmtk/dcmdata/dcrledrg.h" /* for RLE decoder */
#include "dcmtk/dcmjpeg/dipijpeg.h" /* for dcmimage JPEG plugin */
#include "dcmtk/dcmjpeg/djdecode.h" /* for JPEG decoders */
#include "dcmtk/dcmjpls/djdecode.h" /* for JPEG-LS decoders */
#ifdef HAVE_FMJPEG2K_DJDECODE_H
#pragma message("JPEG2000 enabled")
#include "fmjpeg2k/djdecode.h" /* J2K */
#endif

#include "image.h"

void registerCodecs() {
  DJDecoderRegistration::registerCodecs();
  DJLSDecoderRegistration::registerCodecs();
  DcmRLEDecoderRegistration::registerCodecs();
#ifdef HAVE_FMJPEG2K_DJDECODE_H
  FMJPEG2KDecoderRegistration::registerCodecs();
#endif
}

void cleanCodecs() {
  DJDecoderRegistration::cleanup();
  DJLSDecoderRegistration::cleanup();
  DcmRLEDecoderRegistration::cleanup();
#ifdef HAVE_FMJPEG2K_DJDECODE_H
  FMJPEG2KDecoderRegistration::cleanup();
#endif
}

bool getJpeg(std::shared_ptr<DcmFileFormat> ff, char **buff, size_t *buffSize, unsigned long frameNo, unsigned long resizeWidth, unsigned long resizeHeight) {
  std::unique_ptr<DicomImage> image = std::make_unique<DicomImage>(ff->getDataset(), ff->getDataset()->getOriginalXfer(), CIF_UsePartialAccessToPixelData, frameNo, 1);

  if (image == NULL)
    return false;

  if (image->getStatus() != EIS_Normal) {
    // only err i care ..for a now
    OFLOG_ERROR(DCM_dcmdataLogger, "Error: cannot load DICOM image (" << DicomImage::getString(image->getStatus()) << ")");
    return false;
  }

  // dont know why, but img looks better.. dont care anyway
  image->setMinMaxWindow();

  // check..
  // Uint8 *pixelData = (Uint8 *)(image->getOutputData());
  // if (pixelData == NULL)
  //  return false;

  unsigned long h = resizeHeight == 0 ? image->getHeight() : resizeHeight;
  unsigned long w = resizeWidth == 0 ? image->getWidth() : resizeWidth;
  DicomImage *thumb = image->createScaledImage(w, h);
  if (!thumb)
    return false;

  DiJPEGPlugin pl;
  FILE *file = open_memstream(buff, buffSize);
  if (!thumb->writePluginFormat(&pl, file)) {
    fflush(file);
    return false;
  }

  fflush(file);
  return true;
}

std::shared_ptr<void> getFrame(std::shared_ptr<DcmFileFormat> ff, unsigned long frameNo, unsigned long *size) {
  DicomImage img(ff.get(), EXS_Unknown, CIF_UsePartialAccessToPixelData, frameNo, 1);
  if (img.getStatus() != EIS_Normal)
    return NULL;

  const unsigned long mSize = img.getOutputDataSize();
  if (mSize == 0)
    return NULL;

  void *buff = malloc(mSize);
  if (!buff)
    return NULL;

  int status = img.getOutputData(buff, mSize);
  if (!status) {
    free(buff);
    return NULL;
  }

  std::shared_ptr<void> ret(buff, std::free);
  *size = mSize;

  return ret;
}
