/*****************************************************************************
*    Open LiteSpeed is an open source HTTP server.                           *
*    Copyright (C) 2013 - 2020  LiteSpeed Technologies, Inc.                 *
*                                                                            *
*    This program is free software: you can redistribute it and/or modify    *
*    it under the terms of the GNU General Public License as published by    *
*    the Free Software Foundation, either version 3 of the License, or       *
*    (at your option) any later version.                                     *
*                                                                            *
*    This program is distributed in the hope that it will be useful,         *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of          *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the            *
*    GNU General Public License for more details.                            *
*                                                                            *
*    You should have received a copy of the GNU General Public License       *
*    along with this program. If not, see http://www.gnu.org/licenses/.      *
*****************************************************************************/
#ifndef LSLINUXAIOREQ_H
#define LSLINUXAIOREQ_H

#include "edio/linuxaio.h"
#include "edio/lsaioreq.h"

#ifdef LS_AIO_USE_LINUX_AIO

#define LS_AIO_NO_NATIVE_LINUX_AIO

class LinuxAio;

class LsLinuxAioReq : public LsAioReq
{
    struct iocb         m_iocb;

public:
    LsLinuxAioReq(AioReqEventHandler *aioReqEventHandler, int fd);
    ~LsLinuxAioReq();

    virtual int postRead(struct iovec *iov, int iovcnt, off_t pos);
    virtual int getReadResult(struct iovec **iov);
    virtual int  doCancel();

};


#endif // LINUX_AIO
#endif // Guard
