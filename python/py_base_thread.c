/*
    py_base_thread.c

    Copyright (C) 2013 Red Hat, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "py_common.h"
#include "py_base_thread.h"
#include "py_base_frame.h"

#include "thread.h"
#include "frame.h"

#define thread_doc "satyr.BaseThread - base class for threads"
#define frames_doc "A list containing objects representing frames in a thread."

static PyMethodDef
thread_methods[] =
{
    /* methods */
    { NULL },
};

static PyMemberDef
thread_members[] =
{
    { (char *)"frames", T_OBJECT_EX, offsetof(struct sr_py_base_thread, frames), 0, (char *)frames_doc },
    { NULL },
};

PyTypeObject
sr_py_base_thread_type =
{
    PyObject_HEAD_INIT(NULL)
    0,
    "satyr.BaseThread",         /* tp_name */
    sizeof(struct sr_py_base_thread), /* tp_basicsize */
    0,                          /* tp_itemsize */
    NULL,                       /* tp_dealloc */
    NULL,                       /* tp_print */
    NULL,                       /* tp_getattr */
    NULL,                       /* tp_setattr */
    (cmpfunc)sr_py_base_thread_cmp, /* tp_compare */
    NULL,                       /* tp_repr */
    NULL,                       /* tp_as_number */
    NULL,                       /* tp_as_sequence */
    NULL,                       /* tp_as_mapping */
    NULL,                       /* tp_hash */
    NULL,                       /* tp_call */
    NULL,                       /* tp_str */
    NULL,                       /* tp_getattro */
    NULL,                       /* tp_setattro */
    NULL,                       /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    thread_doc,                  /* tp_doc */
    NULL,                       /* tp_traverse */
    NULL,                       /* tp_clear */
    NULL,                       /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    NULL,                       /* tp_iter */
    NULL,                       /* tp_iternext */
    thread_methods,             /* tp_methods */
    thread_members,             /* tp_members */
    NULL,                       /* tp_getset */
    NULL,                       /* tp_base */
    NULL,                       /* tp_dict */
    NULL,                       /* tp_descr_get */
    NULL,                       /* tp_descr_set */
    0,                          /* tp_dictoffset */
    NULL,                       /* tp_init */
    NULL,                       /* tp_alloc */
    NULL,                       /* tp_new */
    NULL,                       /* tp_free */
    NULL,                       /* tp_is_gc */
    NULL,                       /* tp_bases */
    NULL,                       /* tp_mro */
    NULL,                       /* tp_cache */
    NULL,                       /* tp_subclasses */
    NULL,                       /* tp_weaklist */
};

/* helpers */
int
frames_prepare_linked_list(struct sr_py_base_thread *thread)
{
    int i;
    PyObject *item;
    struct sr_py_base_frame *current = NULL, *prev = NULL;

    for (i = 0; i < PyList_Size(thread->frames); ++i)
    {
        item = PyList_GetItem(thread->frames, i);
        if (!item)
            return -1;

        Py_INCREF(item);

        if (!PyObject_TypeCheck(item, thread->frame_type))
        {
            Py_XDECREF(item);
            Py_XDECREF(prev);
            PyErr_Format(PyExc_TypeError, "frames must be a list of %s objects",
                         thread->frame_type->tp_name);
            return -1;
        }

        current = (struct sr_py_base_frame*)item;
        if (i == 0)
            sr_thread_set_frames(thread->thread, current->frame);
        else
            sr_frame_set_next(prev->frame, current->frame);

        Py_XDECREF(prev);
        prev = current;
    }

    if (current)
    {
        sr_frame_set_next(current->frame, NULL);
        Py_DECREF(current);
    }

    return 0;
}

int
frames_free_python_list(struct sr_py_base_thread *thread)
{
    int i;
    PyObject *item;

    for (i = 0; i < PyList_Size(thread->frames); ++i)
    {
        item = PyList_GetItem(thread->frames, i);
        if (!item)
            return -1;
        Py_DECREF(item);
    }
    Py_DECREF(thread->frames);

    return 0;
}

PyObject *
frames_to_python_list(struct sr_thread *thread, PyTypeObject *frame_type)
{
    PyObject *result = PyList_New(0);
    if (!result)
        return NULL;

    struct sr_frame *frame = sr_thread_frames(thread);
    struct sr_py_base_frame *item;
    while (frame)
    {
        item = PyObject_New(struct sr_py_base_frame, frame_type);
        if (!item)
            return PyErr_NoMemory();

        /* XXX may need to initialize item further */
        /* It would be a good idea to have a common code that is executed each
         * time new object (e.g. via __new__ or _dup) is created so that we
         * don't miss setting some attribute. As opposed to using PyObject_New
         * directly. */
        item->frame = frame;
        if (PyList_Append(result, (PyObject *)item) < 0)
            return NULL;

        frame = sr_frame_next(frame);
    }

    return result;
}

/* comparison */
int
sr_py_base_thread_cmp(struct sr_py_base_thread *self, struct sr_py_base_thread *other)
{
    if (self->ob_type != other->ob_type)
    {
        /* distinct types must be unequal */
        return normalize_cmp(self->ob_type - other->ob_type);
    }

    if (frames_prepare_linked_list(self) < 0
        || frames_prepare_linked_list(other) < 0)
    {
        /* exception is already set */
        return -1;
    }

    return normalize_cmp(sr_thread_cmp(self->thread, other->thread));
}