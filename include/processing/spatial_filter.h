/* VideoMagnification - Magnify motions and detect heartbeats
Copyright (C) 2016 Christian Diller

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>. */

#ifndef SPATIAL_FILTER_H
#define SPATIAL_FILTER_H

#include <helpers/data_container.h>

namespace spatial_filter {

    void spatial_decomp(parameter_store& params, DataContainer &data_container);
    void spatial_comp(parameter_store& params, DataContainer& data_container);

}

#endif //SPATIAL_FILTER_H