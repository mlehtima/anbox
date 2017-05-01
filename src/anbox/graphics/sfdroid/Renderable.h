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

#ifndef ANBOX_GRAPHICS_EMUGL_RENDERABLE_H_
#define ANBOX_GRAPHICS_EMUGL_RENDERABLE_H_

#include "anbox/graphics/rect.h"

#include <string>
#include <vector>

#include <cstdint>

class Renderable {
 public:
  Renderable(const std::string &name, void *buffer, int width, int height, int stride, int format);
  ~Renderable();

  std::string name() const;
  int width() const;
  int height() const;
  int stride() const;
  int format() const;
  void *buffer() const;

  //FIXME: Multiwindow mode needs this
  void set_screen_position(const anbox::graphics::Rect &position){ screen_position_  = position; }
  anbox::graphics::Rect screen_position() const { return screen_position_; }
  anbox::graphics::Rect crop() const { return {}; }


  inline bool operator==(const Renderable &rhs) const {
    return (name_ == rhs.name() && buffer_ == rhs.buffer());
  }

  inline bool operator!=(const Renderable &rhs) const {
    return !operator==(rhs);
  }

 private:
  std::string name_;
  anbox::graphics::Rect screen_position_;
  void *buffer_;
  int width_;
  int height_;
  int stride_;
  int format_;
};

std::ostream &operator<<(std::ostream &out, const Renderable &r);

typedef std::vector<Renderable> RenderableList;

#endif

