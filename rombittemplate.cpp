#include "rombittemplate.h"
#include "maskromtool.h"

#include <QtMath>
#include <algorithm>
#include <map>
#include <utility>
#include <vector>

// Returns (2r+1)² offsets sorted by distance from origin, generated once per r.
static const std::vector<std::pair<int,int>> &sortedOffsets(int r) {
    static std::map<int, std::vector<std::pair<int,int>>> cache;
    auto it = cache.find(r);
    if(it != cache.end()) return it->second;
    auto &v = cache[r];
    for(int dy = -r; dy <= r; dy++)
        for(int dx = -r; dx <= r; dx++)
            v.push_back({dx, dy});
    std::sort(v.begin(), v.end(), [](const std::pair<int,int> &a, const std::pair<int,int> &b){
        return a.first*a.first + a.second*a.second
             < b.first*b.first + b.second*b.second;
    });
    return v;
}

static constexpr double EARLY_EXIT_NCC = 0.999;

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

    int64_t sumT=0, sumI=0, sumTT=0, sumII=0, sumTI=0;
    for(int y = 0; y < h; y++) {
        const uchar *lt = T.constScanLine(y);
        const uchar *li = I.constScanLine(y);
        for(int x = 0; x < w; x++) {
            int64_t t = lt[x], iv = li[x];
            sumT += t; sumI += iv; sumTT += t*t; sumII += iv*iv; sumTI += t*iv;
        }
    }
    double n = w * h;
    double num   = (double)sumTI - (double)sumT*(double)sumI/n;
    double denom = sqrt(((double)sumTT - (double)sumT*(double)sumT/n) *
                        ((double)sumII - (double)sumI*(double)sumI/n));
    if(denom < 1e-10) return 0.0;
    return qBound(-1.0, num / denom, 1.0);
}

// Average an accumulator into a Grayscale8 QImage.
static QImage accumToImage(const QVector<float> &acc, int w, int h, int count) {
    QImage img(w, h, QImage::Format_Grayscale8);
    for(int y = 0; y < h; y++) {
        uchar *line = img.scanLine(y);
        for(int x = 0; x < w; x++)
            line[x] = static_cast<uchar>(qBound(0.0f, acc[y*w+x] / count, 255.0f));
    }
    return img;
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

    // === Pass 1: accumulate at nominal position, collect raw images for pass 2 ===
    struct Sample { int key; QImage raw; };
    QVector<Sample> samples;
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

                QImage raw = bit->getImage();
                if(raw.isNull()) { prev = bit; bit = bit->nexttoright; continue; }
                QImage gray = sobelMag(baseCrop(raw).convertToFormat(QImage::Format_Grayscale8));
                if(gray.isNull()) { prev = bit; bit = bit->nexttoright; continue; }

                if(accum[key].isEmpty()) accum[key].fill(0.0f, tw * th);
                for(int y = 0; y < th; y++) {
                    const uchar *line = gray.constScanLine(y);
                    for(int x = 0; x < tw; x++)
                        accum[key][y * tw + x] += line[x];
                }
                counts[key]++;
                totalFixed++;
                samples.append({key, raw});
            }
            prev = bit;
            bit  = bit->nexttoright;
        }
        rowfirst = rowfirst->nextrow;
    }

    // Build rough templates from pass 1.
    for(int k = 0; k < 8; k++) {
        if(counts[k] >= MIN_SAMPLES)
            templates[k] = accumToImage(accum[k], tw, th, counts[k]);
    }

    // === Pass 2: re-accumulate each sample at its best-aligned offset ===
    // For each sample find the offset (within ±SEARCH_RADIUS) that maximises NCC
    // against the rough template, then accumulate the aligned Sobel crop.
    int r = SEARCH_RADIUS;
    QVector<QVector<float>> accum2(8);
    int counts2[8] = {};

    for(const Sample &s : samples) {
        int key = s.key;
        if(!hasTemplate(key)) continue;

        QImage paddedGray = paddedCrop(s.raw).convertToFormat(QImage::Format_Grayscale8);
        QImage sobelPad   = sobelMag(paddedGray);
        if(sobelPad.width() < tw + 2*r || sobelPad.height() < th + 2*r) continue;

        // Find best offset vs rough template.
        int bestDx = 0, bestDy = 0;
        double bestScore = -2.0;
        for(auto [dx, dy] : sortedOffsets(r)) {
            double sc = nccGrayAt(key, sobelPad, r + dx, r + dy);
            if(sc > bestScore) { bestScore = sc; bestDx = dx; bestDy = dy; }
            if(bestScore >= EARLY_EXIT_NCC) break;
        }

        // Accumulate the aligned Sobel crop.
        if(accum2[key].isEmpty()) accum2[key].fill(0.0f, tw * th);
        for(int y = 0; y < th; y++) {
            const uchar *line = sobelPad.constScanLine(r + bestDy + y) + (r + bestDx);
            for(int x = 0; x < tw; x++)
                accum2[key][y * tw + x] += line[x];
        }
        counts2[key]++;
    }

    // Replace rough templates with aligned averages where we have enough samples.
    for(int k = 0; k < 8; k++) {
        if(counts2[k] >= MIN_SAMPLES) {
            templates[k] = accumToImage(accum2[k], tw, th, counts2[k]);
            counts[k] = counts2[k];
        }
    }

    built = false;
    for(int k = 0; k < 8; k++) if(!templates[k].isNull()) { built = true; break; }
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
    int64_t sumT=0, sumI=0, sumTT=0, sumII=0, sumTI=0;
    for(int y = 0; y < h; y++) {
        const uchar *lt = T.constScanLine(y);
        const uchar *li = sobelImg.constScanLine(oy + y) + ox;
        for(int x = 0; x < w; x++) {
            int64_t t = lt[x], iv = li[x];
            sumT += t; sumI += iv; sumTT += t*t; sumII += iv*iv; sumTI += t*iv;
        }
    }
    double n = w * h;
    double num   = (double)sumTI - (double)sumT*(double)sumI/n;
    double denom = sqrt(((double)sumTT - (double)sumT*(double)sumT/n) *
                        ((double)sumII - (double)sumI*(double)sumI/n));
    if(denom < 1e-10) return 0.0;
    return qBound(-1.0, num / denom, 1.0);
}

double RomBitTemplate::nccBest(int key, const QImage &paddedImg) const {
    if(!hasTemplate(key)) return 0.0;
    int r = SEARCH_RADIUS;
    // Sobel the full padded image once, then slide a tw×th window over it.
    QImage sobel = sobelMag(paddedImg.convertToFormat(QImage::Format_Grayscale8));
    double best = -1.0;
    for(auto [dx, dy] : sortedOffsets(r)) {
        double s = nccGrayAt(key, sobel, r + dx, r + dy);
        if(s > best) best = s;
        if(best >= EARLY_EXIT_NCC) break;
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

// Compute Sobel once, run offset search for both key0 and key1.
std::pair<double,double> RomBitTemplate::nccBestBoth(int key, const QImage &paddedImg) const {
    int r = SEARCH_RADIUS;
    QImage sobel = sobelMag(paddedImg.convertToFormat(QImage::Format_Grayscale8));
    int key0 = key & ~2, key1 = key | 2;
    double best0 = -1.0, best1 = -1.0;
    for(auto [dx, dy] : sortedOffsets(r)) {
        if(best0 < EARLY_EXIT_NCC)
            best0 = qMax(best0, nccGrayAt(key0, sobel, r+dx, r+dy));
        if(best1 < EARLY_EXIT_NCC)
            best1 = qMax(best1, nccGrayAt(key1, sobel, r+dx, r+dy));
        if(best0 >= EARLY_EXIT_NCC && best1 >= EARLY_EXIT_NCC) break;
    }
    return {best0, best1};
}

RomBitTemplate::VoteAndScore RomBitTemplate::voteBestWithScore(int key, const QImage &paddedImg) const {
    int key0 = key & ~2, key1 = key | 2;
    bool has0 = hasTemplate(key0), has1 = hasTemplate(key1);
    if(!has0 && !has1) return { (bool)((key >> 1) & 1), 0.0 };
    if(!has0) return { true,  hasTemplate(key1) ? nccBest(key1, paddedImg) : 0.0 };
    if(!has1) return { false, hasTemplate(key0) ? nccBest(key0, paddedImg) : 0.0 };
    auto [s0, s1] = nccBestBoth(key, paddedImg);
    bool vote = s1 > s0;
    // key is always key0 or key1 (center bit = current value); return winning side's score.
    double score = vote ? s1 : s0;
    return { vote, score };
}
