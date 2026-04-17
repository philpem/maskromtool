#ifndef ROMBITTEMPLATE_H
#define ROMBITTEMPLATE_H

#include <QImage>

class MaskRomTool;

/* NCC-based template matcher trained from isFixed() bits.
 * Keys are 3-bit context: (left<<2)|(center<<1)|right.
 *
 * Template dimensions are taken from mrt->sampler->getRect() at build() time,
 * so Wide/Tall/Size settings directly control the crop used for matching.
 *
 * nccBest/voteBest search ±SEARCH_RADIUS pixels around the bit centre
 * to tolerate small grid misalignment.
 */
class RomBitTemplate {
public:
    static const int MIN_SAMPLES = 3;
    static int SEARCH_RADIUS;   // pixel search window around bit centre
    static int TEMPLATE_W;     // crop width  in source pixels (0 = auto from sampler rect)
    static int TEMPLATE_H;     // crop height in source pixels (0 = auto from sampler rect)

    void build(MaskRomTool *mrt);
    bool isBuilt() const;
    bool hasTemplate(int key) const;
    static int makeKey(bool l, bool c, bool r);

    // --- Crop helpers (take a getImage() result as source) ---
    // Center-crop to tw×th.
    QImage baseCrop(const QImage &src) const;
    // Center-crop to (tw+2r)×(th+2r) for offset search.
    QImage paddedCrop(const QImage &src) const;

    // --- Exact-crop NCC (no offset search, used for display) ---
    double ncc(int key, const QImage &img) const;
    bool   vote(int key, const QImage &img) const;

    // --- Offset-search NCC (use in DRC evaluate) ---
    // paddedImg must be (tw+2·SEARCH_RADIUS)×(th+2·SEARCH_RADIUS).
    double nccBest(int key, const QImage &paddedImg) const;
    bool   voteBest(int key, const QImage &paddedImg) const;

    QImage templateImage(int key) const;
    int    sampleCount(int key) const;
    int    fixedBitCount() const;
    int    templateW() const { return tw; }
    int    templateH() const { return th; }

private:
    QImage templates[8];
    int    counts[8] = {0,0,0,0,0,0,0,0};
    int    totalFixed = 0;
    bool   built = false;
    int    tw = 0, th = 0;   // crop/template dimensions in source image pixels

    // NCC assuming grayImg is already tw×th Format_Grayscale8 (Sobel applied internally).
    double nccGray(int key, const QImage &grayImg) const;
    // NCC between template[key] and a tw×th region of a pre-Sobelled image at offset (ox,oy).
    double nccGrayAt(int key, const QImage &sobelImg, int ox, int oy) const;
    // Sobel edge-magnitude image (Format_Grayscale8 → Format_Grayscale8, normalised 0-255).
    static QImage sobelMag(const QImage &gray);
};

#endif // ROMBITTEMPLATE_H
