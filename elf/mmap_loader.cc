// Copyright (c) 2013 Austin T. Clements. All rights reserved.
// Use of this source code is governed by an MIT license
// that can be found in the LICENSE file.

#include "elf++.hh"

#include <system_error>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

#include "external/cpp-mmaplib/mmaplib.h"

using namespace std;

ELFPP_BEGIN_NAMESPACE

class mmap_loader : public loader
{
        void *base;
        size_t lim;
        std::unique_ptr<mmaplib::mmap> _mmap;

public:
        mmap_loader(const char* path)
        {
                _mmap = std::make_unique<mmaplib::mmap>(path);
                lim = _mmap->size();
        }

        const void *load(off_t offset, size_t size)
        {
                if (offset + size > lim)
                        throw range_error("offset exceeds file size");
                return (const char*)_mmap->data() + offset;
        }
};

std::shared_ptr<loader>
create_mmap_loader(const char* path)
{
        return make_shared<mmap_loader>(path);
}

ELFPP_END_NAMESPACE
