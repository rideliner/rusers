/*!
 * Copyright (c) 2014
 * Nathan Currier
 *
 * Use, modification, and distribution are all subject to the
 * Boost Software License, Version 1.0. (See the accompanying
 * file LICENSE.md or at http://rideliner.net/LICENSE.html).
 *
 * <description>
**/

#include "ruby.h"

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <arpa/inet.h>
#include <utmp.h>
#include <stdlib.h>
#include <unistd.h>

#undef ut_time

#include <rpcsvc/rusers.h>

VALUE mRemoteUsers;
VALUE cRemoteUsersInfo;
VALUE cRemoteUsersConn;
VALUE eRPCException;

typedef struct
{
    int login_time,  idle_time;
    const char* host, *user, *remote;
} RemoteUsersInfo;

struct netbuf {
    unsigned int maxlen;
    unsigned int len;
    struct sockaddr *buf;
};

static struct timeval timeout = { 1, 0 };
const char* UnknownUser = "(unknown";

static VALUE remote_users_alloc(VALUE klass);
static VALUE remote_users_init(VALUE self, VALUE hostname, VALUE user, VALUE remote, VALUE login_time, VALUE idle_time);
static void remote_users_free(void* s);
static VALUE remote_users_get(VALUE self, VALUE hostname);
static VALUE remote_users_parse_reply(struct utmpidlearr* up, struct sockaddr_in* raddrp);
void Init_rusers();

static VALUE remote_users_alloc(VALUE klass)
{
    return Data_Wrap_Struct(klass, NULL, remote_users_free, xmalloc(sizeof(RemoteUsersInfo)));
}

static VALUE remote_users_init(VALUE self, VALUE hostname, VALUE user, VALUE remote, VALUE login_time, VALUE idle_time)
{
    RemoteUsersInfo* value;
    Data_Get_Struct(self, RemoteUsersInfo, value);

    value->host = StringValuePtr(hostname);
    value->user = StringValuePtr(user);
    value->remote = StringValuePtr(remote);
    value->login_time = NUM2INT(login_time);
    value->idle_time = NUM2INT(idle_time);

    return self;
}

static void remote_users_free(void* s)
{
    xfree(s);
}

static VALUE remote_users_get(VALUE self, VALUE _hostname)
{
    char* host = StringValuePtr(_hostname);

    struct utmpidlearr up;
    CLIENT* rusers_clnt;
    enum clnt_stat clnt_stat;
    struct sockaddr_in addr;
    struct hostent* hp;

    rusers_clnt = clnt_create(host, RUSERSPROG, RUSERSVERS_IDLE, "udp");
    if (rusers_clnt == NULL)
    {
        rb_raise(eRPCException, "%s", clnt_spcreateerror("rusers"));
        return Qnil;
    }

    memset((char *)&up, 0, sizeof(up));

    clnt_stat = clnt_call(rusers_clnt, RUSERSPROC_NAMES, (xdrproc_t)xdr_void, NULL,
        (xdrproc_t)xdr_utmpidlearr, (caddr_t)&up, timeout);

    if (clnt_stat == RPC_TIMEDOUT)
        rb_raise(eRPCException, "%s", clnt_spcreateerror("rusers"));
    if (clnt_stat != RPC_SUCCESS)
        return Qnil;

    hp = gethostbyname(host);
    addr.sin_addr.s_addr = *(int*)hp->h_addr_list[0];

    return remote_users_parse_reply(&up, &addr);
}

static VALUE remote_users_parse_reply(struct utmpidlearr* up, struct sockaddr_in* raddrp)
{
    // parse data
    VALUE array = rb_ary_new();
    VALUE hostname, user, remote, login_time, idle_time;

    struct hostent* hp;
    hp = gethostbyaddr((char*)&raddrp->sin_addr.s_addr, sizeof(struct in_addr), AF_INET);

    hostname = rb_str_new2(hp ? hp->h_name : inet_ntoa(raddrp->sin_addr));

    for (int i = 0; i < up->uia_cnt; ++i)
    {
        login_time = INT2NUM(up->uia_arr[i]->ui_utmp.ut_time);
        idle_time = INT2NUM(up->uia_arr[i]->ui_idle);
        remote = rb_str_substr(rb_str_new2(up->uia_arr[i]->ui_utmp.ut_host), 0, 16);
        user = rb_str_substr(rb_str_new2(up->uia_arr[i]->ui_utmp.ut_name), 0, 8);

        // throw out invalid users
        if (rb_str_equal(user, rb_str_new2(UnknownUser)))
            continue;

        VALUE args[5] = { hostname, user, remote, login_time, idle_time };
        rb_ary_push(array, rb_class_new_instance(5, args, cRemoteUsersInfo));
    }

    return array;
}

static VALUE remote_users_get_hostname(VALUE self)
{
    RemoteUsersInfo* value;
    Data_Get_Struct(self, RemoteUsersInfo, value);

    return rb_str_new2(value->host);
}

static VALUE remote_users_get_user(VALUE self)
{
    RemoteUsersInfo* value;
    Data_Get_Struct(self, RemoteUsersInfo, value);

    return rb_str_new2(value->user);
}

static VALUE remote_users_get_remote(VALUE self)
{
    RemoteUsersInfo* value;
    Data_Get_Struct(self, RemoteUsersInfo, value);

    return rb_str_new2(value->remote);
}

static VALUE remote_users_get_login_time(VALUE self)
{
    RemoteUsersInfo* value;
    Data_Get_Struct(self, RemoteUsersInfo, value);

    return INT2NUM(value->login_time);
}

static VALUE remote_users_get_idle_time(VALUE self)
{
    RemoteUsersInfo* value;
    Data_Get_Struct(self, RemoteUsersInfo, value);

    return INT2NUM(value->idle_time);
}

void Init_rusers()
{
    mRemoteUsers = rb_define_module("RemoteUsers");
    cRemoteUsersInfo = rb_define_class_under(mRemoteUsers, "Info", rb_cObject);
    cRemoteUsersConn = rb_define_class_under(mRemoteUsers, "Connection", rb_cObject);
    eRPCException = rb_define_class_under(mRemoteUsers, "RPCException", rb_eRuntimeError);

    rb_define_method(cRemoteUsersConn, "getHostInfo", remote_users_get, 1);

    rb_define_alloc_func(cRemoteUsersInfo, remote_users_alloc);
    rb_define_private_method(cRemoteUsersInfo, "initialize", remote_users_init, 5);

    rb_define_method(cRemoteUsersInfo, "hostname", remote_users_get_hostname, 0);
    rb_define_method(cRemoteUsersInfo, "username", remote_users_get_user, 0);
    rb_define_method(cRemoteUsersInfo, "remoteHostname", remote_users_get_remote, 0);
    rb_define_method(cRemoteUsersInfo, "loginTime", remote_users_get_login_time, 0);
    rb_define_method(cRemoteUsersInfo, "idleTime", remote_users_get_idle_time, 0);
}
