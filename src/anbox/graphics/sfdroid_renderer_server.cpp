/*
 * Copyright (C) 2016 Simon Fels <morphis@gravedo.de>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "anbox/graphics/sfdroid_renderer_server.h"
#include "anbox/graphics/sfdroid/Renderer.h"
#include "anbox/graphics/sfdroid/RenderThread.h"
#include "anbox/graphics/layer_composer.h"
#include "anbox/graphics/multi_window_composer_strategy.h"
#include "anbox/graphics/single_window_composer_strategy.h"
#include "anbox/logger.h"
#include "anbox/wm/manager.h"

#include <boost/throw_exception.hpp>
#include <boost/filesystem.hpp>
#include <cstdarg>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/un.h>

using namespace std;

static shared_ptr<RenderThread> render_thread_;

namespace anbox {
namespace graphics {
SFDroidRendererServer::SFDroidRendererServer(const std::string& socket_path, const std::shared_ptr<wm::Manager> &wm, bool single_window)
    : renderer_(std::make_shared<::Renderer>()) {
  create_socket(socket_path);

  std::shared_ptr<LayerComposer::Strategy> composer_strategy;
  if (single_window)
    composer_strategy = std::make_shared<SingleWindowComposerStrategy>(wm);
  else
    composer_strategy = std::make_shared<MultiWindowComposerStrategy>(wm);

  composer_ = std::make_shared<LayerComposer>(renderer_, composer_strategy);

  renderer_->initialize(0);
  // TODO?
  //registerRenderer(renderer_);
  registerLayerComposer(composer_);
}

void SFDroidRendererServer::start_thread(int fd)
{
  render_thread_.reset(RenderThread::create(renderer_));
  render_thread_->set_socket(fd);
  if (!render_thread_->start())
  {
      BOOST_THROW_EXCEPTION(std::runtime_error("Failed to start the renderer thread"));
  }
}

SFDroidRendererServer::~SFDroidRendererServer() { renderer_->finalize(); }

void SFDroidRendererServer::create_socket(const std::string& socket_path)
{
  int sfdroid_connector_socket = -1;
  socket_file_ = utils::string_format("%s/sfdroid_head", socket_path);

  unlink(socket_file_.c_str());
  struct sockaddr_un addr;

  sfdroid_connector_socket = socket(AF_UNIX, SOCK_STREAM, 0);
  if(sfdroid_connector_socket < 0)
  {
    ERROR("failed to create socket");
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_file_.c_str(), sizeof(addr.sun_path) - 1);

  if(bind(sfdroid_connector_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0)
  {
     ERROR("failed to bind socket" + socket_file_);
  }

  start_thread(sfdroid_connector_socket);
}
}  // namespace graphics
}  // namespace anbox
