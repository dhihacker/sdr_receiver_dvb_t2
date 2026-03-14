/*
 *  Copyright 2020 Oleg Malyutin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef SCAN_USB_DEVICE_H
#define SCAN_USB_DEVICE_H

#include <QObject>

class scan_usb_device : public QObject
{
    Q_OBJECT
public:
    explicit scan_usb_device(QObject *parent = nullptr);

signals:
    void found(ushort id_vendor, ushort id_product);

public slots:
    void scan();
};

#endif // SCAN_USB_DEVICE_H
