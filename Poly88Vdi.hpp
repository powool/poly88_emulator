#pragma once

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QPixmap>
#include <QImage>
#include <QColor>

// ---------------------------------------------------------------------------
// Parse XPM C-string array into a QPixmap (10x15, 2 colors, 1 char/pixel)
// ---------------------------------------------------------------------------
static QPixmap pixmapFromXpm(const char **xpm, int width, int height, int scaleW, int scaleH)
{
	// header: "10 15 2 1"
	// color map: index 3 = '.' -> bg, index 4 = '*' -> fg
	QColor bg(BACKGROUND);
	QColor fg(FOREGROUND);

	QImage img(width * scaleW, height * scaleH, QImage::Format_RGB32);
	for (int y = 0; y < height; y++) {
		const char *row = xpm[3 + y]; // skip header + 2 color lines
		for (int x = 0; x < width; x++) {
			for (int xScaleCounter = 0; xScaleCounter < scaleW; xScaleCounter++) {
				for (int yScaleCounter = 0; yScaleCounter < scaleH; yScaleCounter++) {
					img.setPixelColor(x * scaleW + xScaleCounter, y * scaleH + yScaleCounter, (row[x] == '*') ? fg : bg);
				}
			}
		}
	}
	return QPixmap::fromImage(img);
}

// ---------------------------------------------------------------------------
// PolyVdi — QGraphicsView showing 16 rows x 64 columns of characters
// Each character cell is 10 wide x 15 high (with scene height = 240 = 16*15)
// Scene is 640 x 240 pixels, displayed scaled into a 640 x 240 fixed widget
// ---------------------------------------------------------------------------
static constexpr int SCALE_W  = 2;
static constexpr int SCALE_H  = 2;
static constexpr int CHAR_W  = 10;
static constexpr int CHAR_H  = 15;
static constexpr int VDI_COLS = 64;
static constexpr int VDI_ROWS = 16;
static constexpr int SCENE_W = VDI_COLS * CHAR_W * SCALE_W;  // 640
static constexpr int SCENE_H = VDI_ROWS * CHAR_H * SCALE_H;  // 240

class PolyVdi : public QGraphicsView {
	std::unique_ptr<QGraphicsScene> scene;
	std::array<QPixmap, 256> charPixmaps;
	// 16 rows x 64 cols of pixmap items owned by the scene
	std::array<QGraphicsPixmapItem*, VDI_ROWS * VDI_COLS> cells{};
    public:
	void LoadFont() {
		for (int i = 0; i < 256; i++) {
			charPixmaps[i] = pixmapFromXpm(xpmTable[i], CHAR_W, CHAR_H, SCALE_W, SCALE_H);
		}
	}

	PolyVdi() {
		LoadFont();
		scene = std::make_unique<QGraphicsScene>(0, 0, SCENE_W, SCENE_H);
		scene->setBackgroundBrush(QColor(BACKGROUND));

		for (int row = 0; row < VDI_ROWS; row++) {
			for (int col = 0; col < VDI_COLS; col++) {
				auto *item = scene->addPixmap(charPixmaps[0]);
				item->setPos(col * CHAR_W * SCALE_W, row * CHAR_H * SCALE_H);
				cells[row * VDI_COLS + col] = item;
			}
		}

		this->setScene(scene.get());
		this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		this->setFixedSize(SCENE_W + 2, SCENE_H + 2);
		this->setSceneRect(0, 0, SCENE_W, SCENE_H);
	}

	void UpdateScene(std::shared_ptr<EmulatorInterface> emulator) {
		uint16_t videoRAM = 0xf800;
		for (int row = 0; row < VDI_ROWS; row++) {
			for (int col = 0; col < VDI_COLS; col++, videoRAM++) {
				uint8_t ch = emulator->GetMemoryByte(videoRAM);
				cells[row * VDI_COLS + col]->setPixmap(charPixmaps[ch]);
			}
		}
	}
};
