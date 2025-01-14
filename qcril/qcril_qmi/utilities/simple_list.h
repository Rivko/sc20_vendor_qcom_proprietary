/*!
  @file
  simple_list.h

  @brief
  List implemented as a linked list

*/

/*===========================================================================

  Copyright (c) 2016 Qualcomm Technologies, Inc.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.

===========================================================================*/
#ifndef SIMPLE_LIST_H_INCLUDED
#define SIMPLE_LIST_H_INCLUDED

struct simple_list;
struct simple_list_iter;

typedef struct simple_list simple_list;
typedef struct simple_list_iter simple_list_iter;

typedef int (*simple_list_foreach_cb)(simple_list *list, simple_list_iter *iter, void *value, void *user_data);
simple_list *simple_list_new();
void simple_list_append(simple_list *list, void *value);
void simple_list_foreach(simple_list *list, simple_list_foreach_cb cb, void *userdata);
simple_list_iter *simple_list_first(simple_list *list);
simple_list_iter *simple_list_iter_next(simple_list_iter *iter);
void *simple_list_iter_get_value(simple_list_iter *iter);
void simple_list_iter_done(simple_list_iter *iter);
#endif
