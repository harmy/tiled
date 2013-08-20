/*
 * Ascq Tiled Plugin
 *
 * This file is part of Tiled.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ascqplugin.h"

#include "compression.h"
#include "map.h"
#include "mapreader.h"
#include "mapwriter.h"
#include "tile.h"
#include "tilelayer.h"
#include "tileset.h"

#include <QFile>
#include <QFileInfo>
#include <QByteArray>
#include <QDebug>

using namespace Ascq;
using namespace Tiled;


AscqPlugin::AscqPlugin()
{
}

QString AscqPlugin::nameFilter() const
{
	return QString::fromLocal8Bit("《蚩尤传说》地图文件 (*.map)");
}

QString AscqPlugin::errorString() const
{
    return mError;
}


QList<TileLayer*> getObjectLayers(const Tiled::Map *map)
{
	QList<TileLayer*> layers;
	foreach(Layer *layer, map->tileLayers()) {
		if (TileLayer *tl = layer->asTileLayer()) {
			if (tl->name().startsWith(QString::fromLocal8Bit("物件"))) {
				layers.append(tl);
			}
		}
	}
	return layers;
}

TileLayer* mergeLayers(const Tiled::Map *map, const QList<TileLayer*> layers) 
{
	if (layers.empty())
	{
		return NULL;
	}
	
	int width = map->width(), 
		height = map->height();
	TileLayer* merged = new TileLayer("merged", 0, 0, width, height);
	foreach(TileLayer* layer, layers) {
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				Cell cell = layer->cellAt(x, y);
				if (cell.tile && cell.tile->hasProperty("base")) 
				{
					int base = cell.tile->property("base").toInt();
					int base_y = y - (cell.tile->image().height() - base) / 32;

					if (merged->cellAt(x, base_y).isEmpty()) 
					{
						merged->setCell(x, base_y, cell);
					} 
					else 
					{
						if (merged->cellAt(x, base_y - 1).isEmpty())
						{
							merged->setCell(x, base_y - 1, cell);
							cell.tile->setProperty("base", QString::number(cell.tile->image().height() - (y - (base_y - 1)) * 32));
							qDebug() << y << "->" << base_y << " base:" << cell.tile->image().height() - (y - (base_y - 1)) * 32;
						}
						else if (merged->cellAt(x, base_y + 1).isEmpty())
						{
							merged->setCell(x, base_y + 1, cell);
							cell.tile->setProperty("base", QString::number(cell.tile->image().height() - (y - (base_y + 1)) * 32));
						}
						else 
						{
							qDebug() << "cannot find offset";

						}
					}
				}
			}
		}
	}
	return merged;
}

TileLayer* getFloorLayer(const Tiled::Map *map)
{
	foreach(Layer *layer, map->tileLayers()) {
		if (TileLayer *tl = layer->asTileLayer()) {
			if (tl->name().startsWith(QString::fromLocal8Bit("地表"))) {
				return tl;
			}
		}
	}
	return NULL;
}

TileLayer* getBlockLayer(const Tiled::Map *map)
{
	foreach(Layer *layer, map->tileLayers()) {
		if (TileLayer *tl = layer->asTileLayer()) {
			if (tl->name().startsWith(QString::fromLocal8Bit("阻"))) {
				return tl;
			}
		}
	}
	return NULL;
}

TileLayer* getMarkLayer(const Tiled::Map *map)
{
	foreach(Layer *layer, map->tileLayers()) {
		if (TileLayer *tl = layer->asTileLayer()) {
			if (tl->name().startsWith(QString::fromLocal8Bit("遮"))) {
				return tl;
			}
		}
	}
	return NULL;
}

bool AscqPlugin::write(const Tiled::Map *map, const QString &fileName)
{
	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly))
	{
		return false;
	}

	NmpFileHeader nmpHeader;
	memset(&nmpHeader, 0, sizeof(nmpHeader));
	
	QByteArray uncompressed;
	uncompressed.resize(sizeof(nmpHeader));
	nmpHeader.size = 32;
	nmpHeader.version = 100;
	nmpHeader.width = map->width();
	nmpHeader.height = map->height();
	memcpy(uncompressed.data(), &nmpHeader, sizeof(nmpHeader));
	
	//合并物件层
	TileLayer* objectLayer = mergeLayers(map, getObjectLayers(map));
	if (!objectLayer)
	{
		mError = QString::fromLocal8Bit("图层中找不到任何物件层!");
		return false;  
	}
	TileLayer* floorLayer = getFloorLayer(map);
	if (!floorLayer)
	{
		mError = QString::fromLocal8Bit("图层中找不到任何地表层!");
		return false;  
	}
	TileLayer* blockLayer = getBlockLayer(map);
	//TileLayer* markLayer = getMarkLayer(map);
	for (int y = 0; y < map->height(); ++y) {
		for (int x = 0; x < map->width(); ++x) {
			char flag = 0;
			unsigned int imageIndex = 0, base = 0, packIndex = 0;
			Cell floorCell = floorLayer->cellAt(x, y);
			if (!floorCell.isEmpty() && floorCell.tile->hasProperty("imageIndex"))
			{
				flag |= WOOOL_FLAG_TILE;
			}
			Cell objectCell = objectLayer->cellAt(x, y);
			if (!objectCell.isEmpty() && objectCell.tile->hasProperty("imageIndex"))
			{
				flag |= WOOOL_FLAG_OBJECT;
			}
			if (blockLayer && !blockLayer->cellAt(x, y).isEmpty())
			{
				flag |= WOOOL_FLAG_BLOCK;
			}
			

			uncompressed.append(&flag, sizeof(flag));

			if (flag & WOOOL_FLAG_TILE)
			{
				imageIndex = floorCell.tile->property("imageIndex").toInt() - 1;
				uncompressed.append((char*)&imageIndex, sizeof(imageIndex));
				packIndex = floorCell.tile->tileset()->property("packIndex").toInt();
				uncompressed.append((char*)&packIndex, sizeof(packIndex));
			}
			if (flag & WOOOL_FLAG_OBJECT)
			{
				imageIndex = objectCell.tile->property("imageIndex").toInt() - 1;
				uncompressed.append((char*)&imageIndex, sizeof(imageIndex));
				packIndex = objectCell.tile->tileset()->property("packIndex").toInt();
				uncompressed.append((char*)&packIndex, sizeof(packIndex));
				base = objectCell.tile->property("base").toInt();
				uncompressed.append((char*)&base, sizeof(base));
			}
		}
	}

	file.write("ZLIB");
	unsigned int uncompressedLen = uncompressed.length();
	file.write((char*)&uncompressedLen, sizeof(uncompressedLen));
	file.write(compress(uncompressed));
	file.close();

	MapWriter writer;
	QString tmxFileName(fileName);
	tmxFileName.replace(".map", ".tmx");
	writer.writeMap(map, tmxFileName);
	return true;
}

Tiled::Map * Ascq::AscqPlugin::read( const QString &fileName )
{
	using namespace Tiled;

	MapReader reader;
	QString tmxFileName(fileName);
	tmxFileName.replace(".map", ".tmx");
	Map *map = reader.readMap(tmxFileName);
	if (!map)
	{
		mError = QString::fromLocal8Bit("map文件必须和tmx文件在相同的目录下!");
		return 0; 
	}

	QFile f(fileName);
	if (!f.open(QIODevice::ReadOnly)) 
	{
		mError = QString::fromLocal8Bit("map文件打开失败!");
		return 0; 
	}

	QByteArray fileHeader = f.read(4);
	if (fileHeader != "ZLIB")
	{
		mError = QString::fromLocal8Bit("map文件格式有误!");
		return 0; 
	}

	unsigned int uncompressedLen;
	f.read((char*)&uncompressedLen, sizeof(uncompressedLen));
	QByteArray uncompressed;
	uncompressed = decompress(f.readAll(), uncompressedLen);

	NmpFileHeader nmpHeader;
	memset(&nmpHeader, 0, sizeof(nmpHeader));
	memcpy(&nmpHeader, uncompressed.data(), sizeof(nmpHeader));

	QImage metroImage = QImage::fromData(QByteArray::fromBase64("iVBORw0KGgoAAAANSUhEUgAAAEAAAADgCAYAAACzdWAwAAAACXBIWXMAAAsSAAALEgHS3X78AAADH0lEQVR4nO2csWoUURhGv8hAIEgIQqIISpp0ok8gTmNloWBnG0xho+BzCNpYRNLaCVpY2UzwCRS7NEFBTAKyBAmkWht3ncyO7RzZe061N9McDmES9v78C+PxOCVTJcmD9ys17IHw+s6oqVrnmhKBaJI/vwEt6sE1GJrJh26AJLk1nAfCbvtwjrL4XzAALUBjAFqAxgC0AI0BaAEaA9ACNAagBWgMQAvQGIAWoDEALUBjAFqAxgC0AE3fvcBuz8/mlm6AhpAgaQdoKAmShdKvx30JJsnh1kYNeyCsbe85H+B8QM9D5wNKwgC0AI0BaAEaA9ACNAagBWgMQAvQGIAWoDEALUBjAFqAxgC0AI0BaAEaA9ACNM4HdM4NIUHifIDzAYVTJcnDly+e0CIErx49fj59BxwuXrhHygzN2unPt0nnr8D++cs3GJ1hWf/1/dPk88z/AZ9XNlaG1RmW66O9Uftc/EvQALQAjQFoARoD0AI0BqAFaAxAC9AYgBagMQAtQGMAWoDGALQAjQFoARoD0AI0M/cC3e/N550zAdo3JqUwDTC5KysN5wNoAZoqSb49u1jDHghXnh64P8D9AT0P3R9QEgagBWgMQAvQGIAWoDEALUBjAFqAxgC0AI0BaAEaA9ACNAagBWgMQAvQGIAWoHF/QOfcEBIk7g9wPqBwqiTZ3Lxf5P6AnZ03f/cHHJ0uFbU/YHXxZHZ/wNeT5SL2B1xdOv73/oAvx6tzvT/g2vKR+wPaGIAWoDEALUBjAFqAxgC0AI0BaAEaA9ACNAagBWgMQAvQGIAWoDEALUBjAFqAZuZeoPu9+bxzJkD7xqQUpgEmd2Wl4XwALUBTJcmH9Zs17IFwe/+j+wPcH9Dz0P0BJWEAWoDGALQAjQFoARoD0AI0BqAFaAxAC9AYgBagMQAtQGMAWoDGALQAjQFoARr3B3TODSFB4v4A5wMKp0qSS9t3a9gD4cfWO+cDnA/oeeh8QEkYgBagMQAtQGMAWoDGALQAjQFoARoD0AI0BqAFaAxAC9AYgBagMQAtQGMAWoDG+YDOuSEkSJwPKH0+4DeG4XZrdlbEFwAAAABJRU5ErkJggg=="));
	Tileset *metroTileset = new Tileset("metro", 64, 32);
	metroTileset->loadFromImage(metroImage, "");
	if (!map->tilesets().contains(metroTileset))
	{
		map->addTileset(metroTileset);
	}

	TileLayer *blockLayer = getBlockLayer(map);
	if (!blockLayer)
	{
		blockLayer = new TileLayer(QString::fromLocal8Bit("阻挡"), 0, 0, map->width(), map->height());
		int i = sizeof(nmpHeader);
		Tile *tile = metroTileset->tileAt(5);
		for (int y = 0; y < map->height(); ++y) {
			for (int x = 0; x < map->width(); ++x) {
				char flag = 0;
				flag = uncompressed.at(i);
				i += sizeof(char);
				if (flag & WOOOL_FLAG_BLOCK)
				{
					blockLayer->setCell(x, y, Cell(tile));
				}
				if (flag & WOOOL_FLAG_TILE)
				{
					i += 2 * sizeof(unsigned int);
				}
				if (flag & WOOOL_FLAG_OBJECT)
				{
					i += 3 * sizeof(unsigned int);
				}	
			}
		}
		map->addLayer(blockLayer);
	}
	
	return map;
}

bool Ascq::AscqPlugin::supportsFile( const QString &fileName ) const
{
	if (QFileInfo(fileName).suffix() != QLatin1String("map"))
		return false;

	QFile f(fileName);
	if (!f.open(QIODevice::ReadOnly))
		return false;
	return f.read(4) == "ZLIB";
}



#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(Ascq, AscqPlugin)
#endif

