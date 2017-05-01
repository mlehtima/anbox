/*
* Copyright (C) 2016 Simon Fels <morphis@gravedo.de>
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "Renderable.h"

Renderable::Renderable(const std::string &name, void *buffer, int width, int height, int stride, int format)
    : name_(name),
      buffer_(buffer),
      width_(width),
      height_(height),
      stride_(stride),
      format_(format) {}

Renderable::~Renderable() {}

std::string Renderable::name() const { return name_; }

void *Renderable::buffer() const { return buffer_; }

int Renderable::width() const { return width_; }
int Renderable::height() const { return height_; }
int Renderable::stride() const { return stride_; }
int Renderable::format() const { return format_; }

std::ostream &operator<<(std::ostream &out, const Renderable &r) {
  return out << "{ name " << r.name() << " buffer " << r.buffer();
}

