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

#ifndef ASCQPLUGIN_H
#define ASCQPLUGIN_H

#include "ascq_global.h"

#include "mapwriterinterface.h"
#include "mapreaderinterface.h"

#include <QObject>
#include <QMap>

namespace Ascq {

//地图文件头结构
struct NmpFileHeader {
	unsigned int	size;
	unsigned int	version;
	unsigned int	width;
	unsigned int	height;
	char			unknown[16];
};

enum WooolMapFlag {
	WOOOL_FLAG_BLOCK		= 1,
	WOOOL_FLAG_SMALLTILE	= 2,
	WOOOL_FLAG_TILE			= 4,
	WOOOL_FLAG_OBJECT		= 8,
	WOOOL_FLAG_UNKNOW		= 16,
};

class ASCQSHARED_EXPORT AscqPlugin
        : public QObject
        , public Tiled::MapWriterInterface
		, public Tiled::MapReaderInterface
{
    Q_OBJECT
    Q_INTERFACES(Tiled::MapWriterInterface)
	Q_INTERFACES(Tiled::MapReaderInterface)

#if QT_VERSION >= 0x050000
    Q_PLUGIN_METADATA(IID "org.mapeditor.MapWriterInterface" FILE "plugin.json")
	Q_PLUGIN_METADATA(IID "org.mapeditor.MapReaderInterface" FILE "plugin.json")
#endif

public:
    AscqPlugin();

	// MapReaderInterface
	Tiled::Map *read(const QString &fileName);
	bool supportsFile(const QString &fileName) const;

    // MapWriterInterface
    bool write(const Tiled::Map *map, const QString &fileName);
    QString nameFilter() const;
    QString errorString() const;

private:
    QString mError;
};

} // namespace Ascq

#endif // ASCQPLUGIN_H
