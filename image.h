#include "dcmtk/dcmdata/dcfilefo.h"

void registerCodecs();
void cleanCodecs();

bool getJpeg(std::shared_ptr<DcmFileFormat> ff, char **buff, size_t *buffSize, unsigned long  frameNo, unsigned long resizeWidth, unsigned long resizeHeight);
std::shared_ptr<void> getFrame(std::shared_ptr<DcmFileFormat> ff, unsigned long frameNo, unsigned long *size);
