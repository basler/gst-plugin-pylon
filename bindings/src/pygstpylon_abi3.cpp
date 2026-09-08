/* Copyright (C) 2026 Basler AG
 *
 * CPython stable-ABI bindings for GstPylonMeta.
 */

#define Py_LIMITED_API 0x030A0000
#include "config.h"

#include <Python.h>
#include <gst/pylon/gstpylonmeta.h>

#include <cstdint>

typedef struct {
  PyObject_HEAD GstPylonMeta *meta;
} PyGstPylonMeta;

static PyObject *gst_pylon_meta_new(PyTypeObject *type, PyObject *,
                                    PyObject *) {
  auto *self = reinterpret_cast<PyGstPylonMeta *>(PyType_GenericAlloc(type, 0));
  if (self == nullptr) {
    return nullptr;
  }

  self->meta = g_new0(GstPylonMeta, 1);
  if (self->meta == nullptr) {
    Py_DECREF(reinterpret_cast<PyObject *>(self));
    return PyErr_NoMemory();
  }
  return reinterpret_cast<PyObject *>(self);
}

static void gst_pylon_meta_dealloc(PyObject *object) {
  auto *self = reinterpret_cast<PyGstPylonMeta *>(object);
  if (self->meta != nullptr) {
    if (self->meta->chunks != nullptr) {
      gst_structure_free(self->meta->chunks);
    }
    g_free(self->meta);
  }
  PyTypeObject *type = Py_TYPE(object);
  PyObject_Free(object);
  Py_DECREF(reinterpret_cast<PyObject *>(type));
}

static GstPylonMeta *checked_meta(PyObject *object) {
  auto *self = reinterpret_cast<PyGstPylonMeta *>(object);
  if (self->meta == nullptr) {
    PyErr_SetString(PyExc_RuntimeError, "GstPylonMeta is no longer valid");
    return nullptr;
  }
  return self->meta;
}

#define UINT64_GETTER(name, member)                   \
  static PyObject *name(PyObject *object, void *) {   \
    GstPylonMeta *meta = checked_meta(object);        \
    if (meta == nullptr) {                            \
      return nullptr;                                 \
    }                                                 \
    return PyLong_FromUnsignedLongLong(meta->member); \
  }

UINT64_GETTER(get_block_id, block_id)
UINT64_GETTER(get_image_number, image_number)
UINT64_GETTER(get_skipped_images, skipped_images)
UINT64_GETTER(get_timestamp, timestamp)
UINT64_GETTER(get_stride, stride)
UINT64_GETTER(get_offset_x, offset.offset_x)
UINT64_GETTER(get_offset_y, offset.offset_y)

#undef UINT64_GETTER

static PyObject *get_chunks(PyObject *object, void *) {
  GstPylonMeta *meta = checked_meta(object);
  if (meta == nullptr) {
    return nullptr;
  }

  PyObject *result = PyDict_New();
  if (result == nullptr || meta->chunks == nullptr) {
    return result;
  }

  for (int index = 0; index < gst_structure_n_fields(meta->chunks); ++index) {
    const gchar *name = gst_structure_nth_field_name(meta->chunks, index);
    const GType type = gst_structure_get_field_type(meta->chunks, name);
    PyObject *value = nullptr;

    if (type == G_TYPE_INT64) {
      gint64 chunk = 0;
      if (gst_structure_get_int64(meta->chunks, name, &chunk)) {
        value = PyLong_FromLongLong(chunk);
      }
    } else if (type == G_TYPE_DOUBLE) {
      gdouble chunk = 0.0;
      if (gst_structure_get_double(meta->chunks, name, &chunk)) {
        value = PyFloat_FromDouble(chunk);
      }
    }

    if (value != nullptr) {
      if (PyDict_SetItemString(result, name, value) < 0) {
        Py_DECREF(value);
        Py_DECREF(result);
        return nullptr;
      }
      Py_DECREF(value);
    } else if (PyErr_Occurred()) {
      Py_DECREF(result);
      return nullptr;
    }
  }
  return result;
}

static PyGetSetDef gst_pylon_meta_getset[] = {
    {const_cast<char *>("block_id"), get_block_id, nullptr, nullptr, nullptr},
    {const_cast<char *>("image_number"), get_image_number, nullptr, nullptr,
     nullptr},
    {const_cast<char *>("skipped_images"), get_skipped_images, nullptr, nullptr,
     nullptr},
    {const_cast<char *>("timestamp"), get_timestamp, nullptr, nullptr, nullptr},
    {const_cast<char *>("stride"), get_stride, nullptr, nullptr, nullptr},
    {const_cast<char *>("offset_x"), get_offset_x, nullptr, nullptr, nullptr},
    {const_cast<char *>("offset_y"), get_offset_y, nullptr, nullptr, nullptr},
    {const_cast<char *>("chunks"), get_chunks, nullptr, nullptr, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr},
};

static PyType_Slot gst_pylon_meta_slots[] = {
    {Py_tp_new, reinterpret_cast<void *>(gst_pylon_meta_new)},
    {Py_tp_dealloc, reinterpret_cast<void *>(gst_pylon_meta_dealloc)},
    {Py_tp_getset, gst_pylon_meta_getset},
    {0, nullptr},
};

static PyType_Spec gst_pylon_meta_spec = {
    "pygstpylon.GstPylonMeta", sizeof(PyGstPylonMeta), 0,
    Py_TPFLAGS_DEFAULT,        gst_pylon_meta_slots,
};

static PyObject *gst_buffer_get_pylon_meta_py(PyObject *module,
                                              PyObject *args) {
  unsigned long long address = 0;
  if (!PyArg_ParseTuple(args, "K:gst_buffer_get_pylon_meta", &address)) {
    return nullptr;
  }

  auto *buffer = reinterpret_cast<GstBuffer *>(static_cast<uintptr_t>(address));
  GstPylonMeta *meta = gst_buffer_get_pylon_meta(buffer);
  if (meta == nullptr) {
    Py_RETURN_NONE;
  }

  PyObject *type = PyObject_GetAttrString(module, "GstPylonMeta");
  if (type == nullptr) {
    return nullptr;
  }
  PyObject *object = PyObject_CallNoArgs(type);
  Py_DECREF(type);
  if (object == nullptr) {
    return nullptr;
  }

  // Return an owned snapshot. The GstBuffer may be released immediately after
  // a pad probe or appsink callback, so retaining a pointer into its metadata
  // would leave the Python object dangling.
  auto *wrapper = reinterpret_cast<PyGstPylonMeta *>(object);
  wrapper->meta->block_id = meta->block_id;
  wrapper->meta->image_number = meta->image_number;
  wrapper->meta->skipped_images = meta->skipped_images;
  wrapper->meta->offset = meta->offset;
  wrapper->meta->timestamp = meta->timestamp;
  wrapper->meta->stride = meta->stride;
  if (meta->chunks != nullptr) {
    wrapper->meta->chunks = gst_structure_copy(meta->chunks);
    if (wrapper->meta->chunks == nullptr) {
      Py_DECREF(object);
      return PyErr_NoMemory();
    }
  }
  return object;
}

static PyMethodDef module_methods[] = {
    {"gst_buffer_get_pylon_meta", gst_buffer_get_pylon_meta_py, METH_VARARGS,
     "Return GstPylonMeta attached to a GstBuffer address."},
    {nullptr, nullptr, 0, nullptr},
};

static PyModuleDef module_definition = {
    PyModuleDef_HEAD_INIT,
    "pygstpylon",
    "Basler gstreamer pylonsrc access package",
    -1,
    module_methods,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

PyMODINIT_FUNC PyInit_pygstpylon(void);

PyMODINIT_FUNC PyInit_pygstpylon(void) {
  PyObject *module = PyModule_Create(&module_definition);
  if (module == nullptr) {
    return nullptr;
  }

  PyObject *meta_type = PyType_FromSpec(&gst_pylon_meta_spec);
  if (meta_type == nullptr) {
    Py_DECREF(module);
    return nullptr;
  }
  if (PyModule_AddObject(module, "GstPylonMeta", meta_type) < 0) {
    Py_XDECREF(meta_type);
    Py_DECREF(module);
    return nullptr;
  }
  if (PyModule_AddStringConstant(module, "__version__", PACKAGE_VERSION) < 0) {
    Py_DECREF(module);
    return nullptr;
  }
  return module;
}
