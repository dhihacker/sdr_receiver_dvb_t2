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
#ifndef UPLOAD_SDRUSBGADGET_H
#define UPLOAD_SDRUSBGADGET_H

#include <string>

class upload_sdrusbgadget
{
public:
    upload_sdrusbgadget();
    void upload(std::string ip);
};

#endif // UPLOAD_SDRUSBGADGET_H
