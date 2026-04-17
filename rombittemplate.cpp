#include "rombittemplate.h"
#include "maskromtool.h"

#include <QtMath>

int RomBitTemplate::SEARCH_RADIUS   = 2;
int RomBitTemplate::TEMPLATE_W      = 0;
int RomBitTemplate::TEMPLATE_H      = 0;

int RomBitTemplate::makeKey(bool l, bool c, bool r) {
    return (l ? 4 : 0) | (c ? 2 : 0) | (r ? 1 : 0);
}

bool RomBitTemplate::isBuilt() const { return built; }

bool RomBitTemplate::hasTemplate(int key) const {
    return key >= 0 && key < 8 && !templates[key].isNull();
}

int RomBitTemplate::sampleCount(int key) const {
    return (key >= 0 && key < 8) ? counts[key] : 0;
}

int RomBitTemplate::fixedBitCount() const { return totalFixed; }

QImage RomBitTemplate::templateImage(int key) const {
    return (key >= 0 && key < 8) ? templates[key] : QImage();
}

// Center-crop src to tw×th (source pixels).  QImage::copy fills any
// out-of-bounds region with black, but with sane W/H values this never fires.
QImage RomBitTemplate::baseCrop(const QImage &src) const {
    int cx = src.width()  / 2;
    int cy = src.height() / 2;
    return src.copy(cx - tw/2, cy - th/2, tw, th);
}

// Center-crop src to (tw+2r)×(th+2r) for offset search.
QImage RomBitTemplate::paddedCrop(const QImage &src) const {
    int r  = SEARCH_RADIUS;
    int pw = tw + 2 * r;
    int ph = th + 2 * r;
    int cx = src.width()  / 2;
    int cy = src.height() / 2;
    return src.copy(cx - pw/2, cy - ph/2, pw, ph);
}

// Sobel gradient magnitude, normalised to 0-255.
QImage RomBitTemplate::sobelMag(const QImage &gray) {
    int w = gray.width(), h = gray.height();
    QImage out(w, h, QImage::Format_Grayscale8);

    auto px = [&](int x, int y) -> int {
        return gray.constScanLine(qBound(0,y,h-1))[qBound(0,x,w-1)];
    };

    QVector<int> mag(w * h);
    int maxMag = 1;
    for(int y = 0; y < h; y++) {
        for(int x = 0; x < w; x++) {
            int gx = -px(x-1,y-1) + px(x+1,y-1)
                     -2*px(x-1,y) + 2*px(x+1,y)
                     -px(x-1,y+1) + px(x+1,y+1);
            int gy = -px(x-1,y-1) - 2*px(x,y-1) - px(x+1,y-1)
                     +px(x-1,y+1) + 2*px(x,y+1) + px(x+1,y+1);
            int m = qAbs(gx) + qAbs(gy);
            mag[y * w + x] = m;
            if(m > maxMag) maxMag = m;
        }
    }
    for(int y = 0; y < h; y++) {
        uchar *line = out.scanLine(y);
        for(int x = 0; x < w; x++)
            line[x] = static_cast<uchar>(mag[y * w + x] * 255 / maxMag);
    }
    return out;
}

// Private: NCC assuming grayImg is already tw×th Format_Grayscale8.
double RomBitTemplate::nccGray(int key, const QImage &grayImg) const {
    const QImage &T = templates[key];
    int w = T.width(), h = T.height();

    // Safe resize fallback if sizes diverge (e.g. after SEARCH_RADIUS change).
    QImage I = grayImg;
    if(I.width() != w || I.height() != h)
        I = I.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
             .convertToFormat(QImage::Format_Grayscale8);
    // Templates are stored as Sobel edge maps; apply the same to the query.
    I = sobelMag(I);

    // Single-pass NCC: accumulate sumT, sumI, sumTT, sumII, sumTI then derive.
    double sumT = 0.0, sumI = 0.0, sumTT = 0.0, sumII = 0.0, sumTI = 0.0;
    for(int y = 0; y < h; y++) {
        const uchar *lt = T.constScanLine(y);
        const uchar *li = I.constScanLine(y);
        for(int x = 0; x < w; x++) {
            double t = lt[x], i = li[x];
            sumT += t; sumI += i; sumTT += t*t; sumII += i*i; sumTI += t*i;
        }
    }
    double n = w * h;
    double num   = sumTI - sumT * sumI / n;
    double denom = sqrt((sumTT - sumT*sumT/n) * (sumII - sumI*sumI/n));
    if(denom < 1e-10) return 0.0;
    return qBound(-1.0, num / denom, 1.0);
}

void RomBitTemplate::build(MaskRomTool *mrt) {
    for(int i = 0; i < 8; i++) { templates[i] = QImage(); counts[i] = 0; }
    totalFixed = 0;
    built = false;
    tw = 0; th = 0;

    // tw/th = crop dimensions in source image pixels.
    // Explicit override takes precedence; otherwise use the sampler rect size.
    if(TEMPLATE_W > 0 && TEMPLATE_H > 0) {
        tw = TEMPLATE_W;
        th = TEMPLATE_H;
    } else {
        QRectF sampRect = mrt->sampler->getRect(mrt);
        tw = qMax(1, (int)qRound(qAbs(sampRect.width())));
        th = qMax(1, (int)qRound(qAbs(sampRect.height())));
    }

    QVector<QVector<float>> accum(8);

    RomBitItem *rowfirst = mrt->markBitTable();
    if(!rowfirst) return;

    while(rowfirst) {
        RomBitItem *prev = nullptr;
        RomBitItem *bit  = rowfirst;
        while(bit) {
            if(bit->isFixed()) {
                bool leftVal  = prev ? prev->bitValue() : bit->bitValue();
                RomBitItem *next = bit->nexttoright;
                bool rightVal = next ? next->bitValue() : bit->bitValue();
                int key = makeKey(leftVal, bit->bitValue(), rightVal);

                QImage raw  = bit->getImage();
                if(raw.isNull()) { prev = bit; bit = bit->nexttoright; continue; }
                QImage gray = sobelMag(baseCrop(raw).convertToFormat(QImage::Format_Grayscale8));
                if(gray.isNull()) { prev = bit; bit = bit->nexttoright; continue; }

                if(accum[key].isEmpty())
                    accum[key].fill(0.0f, tw * th);
                for(int y = 0; y < th; y++) {
                    const uchar *line = gray.constScanLine(y);
                    for(int x = 0; x < tw; x++)
                        accum[key][y * tw + x] += line[x];
                }
                counts[key]++;
                totalFixed++;
            }
            prev = bit;
            bit  = bit->nexttoright;
        }
        rowfirst = rowfirst->nextrow;
    }

    for(int k = 0; k < 8; k++) {
        if(counts[k] >= MIN_SAMPLES) {
            QImage img(tw, th, QImage::Format_Grayscale8);
            for(int y = 0; y < th; y++) {
                uchar *line = img.scanLine(y);
                for(int x = 0; x < tw; x++)
                    line[x] = static_cast<uchar>(
                        qBound(0.0f, accum[k][y * tw + x] / counts[k], 255.0f));
            }
            templates[k] = img;
        }
    }
    built = (tw > 0 && th > 0);
}

// --- Exact-crop NCC (resizes img to match template) ---

double RomBitTemplate::ncc(int key, const QImage &img) const {
    if(!hasTemplate(key)) return 0.0;
    QImage gray = baseCrop(img).convertToFormat(QImage::Format_Grayscale8);
    return nccGray(key, gray);
}

bool RomBitTemplate::vote(int key, const QImage &img) const {
    int key0 = key & ~2, key1 = key | 2;
    bool has0 = hasTemplate(key0), has1 = hasTemplate(key1);
    if(!has0 && !has1) return (key >> 1) & 1;
    if(!has0) return true;
    if(!has1) return false;
    return ncc(key1, img) > ncc(key0, img);
}

// --- Offset-search NCC ---

// NCC between template[key] and a tw×th region of a pre-Sobelled image at (ox,oy).
double RomBitTemplate::nccGrayAt(int key, const QImage &sobelImg, int ox, int oy) const {
    const QImage &T = templates[key];
    int w = T.width(), h = T.height();
    double sumT = 0, sumI = 0, sumTT = 0, sumII = 0, sumTI = 0;
    for(int y = 0; y < h; y++) {
        const uchar *lt = T.constScanLine(y);
        const uchar *li = sobelImg.constScanLine(oy + y) + ox;
        for(int x = 0; x < w; x++) {
            double t = lt[x], i = li[x];
            sumT += t; sumI += i; sumTT += t*t; sumII += i*i; sumTI += t*i;
        }
    }
    double n = w * h;
    double num   = sumTI - sumT * sumI / n;
    double denom = sqrt((sumTT - sumT*sumT/n) * (sumII - sumI*sumI/n));
    if(denom < 1e-10) return 0.0;
    return qBound(-1.0, num / denom, 1.0);
}

double RomBitTemplate::nccBest(int key, const QImage &paddedImg) const {
    if(!hasTemplate(key)) return 0.0;
    int r = SEARCH_RADIUS;
    // Sobel the full padded image once, then slide a tw×th window over it.
    QImage sobel = sobelMag(paddedImg.convertToFormat(QImage::Format_Grayscale8));
    double best = -1.0;
    for(int dy = -r; dy <= r; dy++) {
        for(int dx = -r; dx <= r; dx++) {
            double s = nccGrayAt(key, sobel, r + dx, r + dy);
            if(s > best) best = s;
        }
    }
    return best;
}

bool RomBitTemplate::voteBest(int key, const QImage &paddedImg) const {
    int key0 = key & ~2, key1 = key | 2;
    bool has0 = hasTemplate(key0), has1 = hasTemplate(key1);
    if(!has0 && !has1) return (key >> 1) & 1;
    if(!has0) return true;
    if(!has1) return false;
    return nccBest(key1, paddedImg) > nccBest(key0, paddedImg);
}
