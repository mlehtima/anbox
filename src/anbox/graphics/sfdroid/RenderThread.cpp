/*
* Copyright (C) 2011 The Android Open Source Project
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
#include "RenderThread.h"

#include "Renderer.h"

#include "../../../shared/OpenglCodecCommon/ChecksumCalculatorThreadInfo.h"

#include "anbox/logger.h"

#include <string.h>

#include <sys/socket.h>
#include <cstdlib>
#include <iostream>
#include <string.h>
#include <unistd.h>

#define HWC_LAYER_NAME_MAX_LENGTH 128

using namespace std;

#define STREAM_BUFFER_SIZE 4 * 1024 * 1024

#define MAX_NUM_FDS 32
#define MAX_NUM_INTS 32

static std::shared_ptr<anbox::graphics::LayerComposer> composer;

void registerLayerComposer(
    const std::shared_ptr<anbox::graphics::LayerComposer> &c) {
  composer = c;
}

RenderThread::RenderThread(const std::shared_ptr<Renderer> &renderer)
    : emugl::Thread(), renderer_(renderer), force_stop_(false) {}

RenderThread::~RenderThread() {
  forceStop();
}

RenderThread *RenderThread::create(const std::shared_ptr<Renderer> &renderer) {
  return new RenderThread(renderer);
}

void RenderThread::forceStop() { force_stop_ = true; }

int native_handle_close(const native_handle_t* h)
{
    if (h->version != sizeof(native_handle_t))
        return -EINVAL;
    const int numFds = h->numFds;
    int i;
    for (i=0 ; i<numFds ; i++) {
        close(h->data[i]);
    }
    return 0;
}

int recv_native_handle(int fd, native_handle_t **handle, struct buffer_info_t *info, unsigned int fds, unsigned int ints)
{
    struct msghdr socket_message;
    struct iovec io_vector[1];
    struct cmsghdr *control_message = NULL;
    unsigned int buffer_size = sizeof(struct buffer_info_t) + sizeof(native_handle_t) + sizeof(int)*(fds + ints);
    unsigned int handle_size = sizeof(native_handle_t) + sizeof(int)*(fds + ints);
    char message_buffer[buffer_size];
    char ancillary_buffer[CMSG_SPACE(sizeof(int) * fds)];

    *handle = (native_handle_t*)malloc(handle_size);
    if(!(*handle)) return -1;

    memset(&socket_message, 0, sizeof(struct msghdr));
    memset(ancillary_buffer, 0, CMSG_SPACE(sizeof(int) * fds));

    io_vector[0].iov_base = message_buffer;
    io_vector[0].iov_len = buffer_size;
    socket_message.msg_iov = io_vector;
    socket_message.msg_iovlen = 1;

    socket_message.msg_control = ancillary_buffer;
    socket_message.msg_controllen = CMSG_SPACE(sizeof(int) * fds);

    control_message = CMSG_FIRSTHDR(&socket_message);
    control_message->cmsg_len = socket_message.msg_controllen;
    control_message->cmsg_level = SOL_SOCKET;
    control_message->cmsg_type = SCM_RIGHTS;

    if(recvmsg(fd, &socket_message, MSG_CMSG_CLOEXEC) < 0)
    {
        if(errno != ETIMEDOUT && errno != EAGAIN) cerr << "recvmsg failed: " << strerror(errno) << endl;
        free(*handle);
        *handle = NULL;
        return -1;
    }

    memcpy(info, message_buffer, sizeof(struct buffer_info_t));

    memcpy(*handle, message_buffer + sizeof(struct buffer_info_t), sizeof(native_handle_t));

    if((unsigned int)(*handle)->numFds > MAX_NUM_FDS)
    {
        cerr << "too less space reserved for fds: " << (*handle)->numFds << " > " << MAX_NUM_FDS << endl;
        free(*handle);
        *handle = NULL;
        return -1;
    }

    if((unsigned int)(*handle)->numInts > MAX_NUM_INTS)
    {
        cerr << "too less space reserved for ints: " << (*handle)->numInts << " > " << MAX_NUM_INTS << endl;
        free(*handle);
        *handle = NULL;
        return -1;
    }

    if(ints != (*handle)->numInts)
    {
        cerr << "received wrong number of ints" << endl;
    }

    if(fds != (*handle)->numFds)
    {
        cerr << "received wrong number of fds" << endl;
    }

    *handle = (native_handle_t*)realloc(*handle, sizeof(native_handle_t) + sizeof(int)*((*handle)->numFds + (*handle)->numInts));

    if(*handle == NULL)
    {
        cerr << "not enough memory" << endl;
        return -1;
    }

    memcpy((char*)*handle + sizeof(native_handle_t), message_buffer + sizeof(struct buffer_info_t) + sizeof(native_handle_t), sizeof(int)*((*handle)->numFds + (*handle)->numInts));

    if(socket_message.msg_flags & MSG_CTRUNC)
    {
        cerr << "not enough space in the ancillary buffer" << endl;
        free(*handle);
        *handle = NULL;
        return -1;
    }

    for(int i=0;i<(*handle)->numFds;i++)
    {
        ((native_handle_t*)(*handle))->data[i] = ((int*)CMSG_DATA(control_message))[i];
    }

    return 0;
}

void RenderThread::set_socket(int fd)
{
    sfdroid_socket = fd;
}

intptr_t RenderThread::main() {
  int fd;
  native_handle_t *handle;
  buffer_info_t info;
  char num_of_fdsints[2];
  char syncbyte;

  listen(sfdroid_socket, 5);

  while(!force_stop_) {
    fd = accept(sfdroid_socket, NULL, NULL);
    if(fd >= 0) {
      while(!force_stop_) {
        if(recv(fd, &syncbyte, 1, 0) != 1) {
            std::cout << "failed to read sync byte" << endl;
            close(fd);
            fd = -1;
            break;
        }

        if(syncbyte != 0xAA) {
            std::cout << "invalid sync byte" << endl;
            close(fd);
            fd = -1;
            break;
        }

        if(recv(fd, &num_of_fdsints[0], 2, 0) != 2)
        {
            std::cout << "failed to read payload size" << endl;
            close(fd);
            fd = -1;
            break;
        }

        if(recv_native_handle(fd, &handle, &info, num_of_fdsints[0], num_of_fdsints[1]) == 0) {
          std::cout << handle << std::endl;
          std::vector<Renderable> frame_layers;
          info.layer_name[HWC_LAYER_NAME_MAX_LENGTH - 1] = 0;
          frame_layers.push_back({info.layer_name, handle, info.width, info.height, info.stride, info.format});

          if (composer) {
            std::cout << "submitting layer" << info.layer_name << std::endl;
            composer->submit_layers(frame_layers);
          }

          native_handle_close(handle);

          if(send(fd, &syncbyte, 1, 0) != 1) {
              std::cout << "failed to tell hwcomposer that we're done" << std::endl;
              close(fd);
              fd = -1;
              break;
          }
        } else {
            std::cout << "failed to recv native handle" << endl;
            close(fd);
            fd = -1;
        }
      }
    }
  }

  return 0;
}
