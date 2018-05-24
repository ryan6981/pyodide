#include "js2python.hpp"

#include "jsproxy.hpp"
#include "pyproxy.hpp"

using emscripten::val;

static val *Array = NULL;
static val *Object = NULL;

PyObject *jsToPython(val x) {
  val xType = x.typeOf();

  // String
  if (xType.equals(val("string"))) {
    std::wstring x_str = x.as<std::wstring>();
    return PyUnicode_FromWideChar(&*x_str.begin(), x_str.size());

  // Number
  } else if (xType.equals(val("number"))) {
    double x_double = x.as<double>();
    return PyFloat_FromDouble(x_double);

  // undefined and null (both map to Python None)
  } else if (x.isUndefined() || x.isNull()) {
    Py_INCREF(Py_None);
    return Py_None;

  // true
  } else if (x.isTrue()) {
    Py_INCREF(Py_True);
    return Py_True;

  // false
  } else if (x.isFalse()) {
    Py_INCREF(Py_False);
    return Py_False;

  // raw PyObject pointer
  } else if (!x["$$"].isUndefined() &&
             x["$$"]["ptrType"]["name"].equals(val("PyObject*"))) {
    PyObject *py_x = x.as<PyObject *>(emscripten::allow_raw_pointers());
    Py_INCREF(py_x);
    return py_x;

  // callable PyObject pointer
  } else if (!x["$$"].isUndefined() &&
             x["$$"]["ptrType"]["name"].equals(val("PyCallable*"))) {
    PyCallable py_x = x.as<PyCallable>();
    PyObject *pypy_x = py_x.x;
    Py_INCREF(pypy_x);
    return pypy_x;

  // byte string
  } else if (!x["byteLength"].isUndefined()) {
    std::string x_str = x.as<std::string>();
    return PyBytes_FromStringAndSize(x_str.c_str(), x_str.size());

  // everything else: return a JsProxy
  } else {
    return JsProxy_cnew(x);
  }
}

PyObject *jsToPythonArgs(val args) {
  if (!Array->call<bool>("isArray", args)) {
    PyErr_SetString(PyExc_TypeError, "Invalid args");
    return NULL;
  }

  Py_ssize_t n = (Py_ssize_t)args["length"].as<long>();
  PyObject *pyargs = PyTuple_New(n);
  if (pyargs == NULL) {
    return NULL;
  }

  for (Py_ssize_t i = 0; i < n; ++i) {
    PyObject *arg = jsToPython(args[i]);
    PyTuple_SET_ITEM(pyargs, i, arg);
  }

  return pyargs;
}

PyObject *jsToPythonKwargs(val kwargs) {
  val keys = Object->call<val>("keys", kwargs);

  Py_ssize_t n = (Py_ssize_t)keys["length"].as<long>();
  PyObject *pykwargs = PyDict_New();
  if (pykwargs == NULL) {
    return NULL;
  }

  for (Py_ssize_t i = 0; i < n; ++i) {
    PyObject *k = jsToPython(keys[i]);
    PyObject *v = jsToPython(kwargs[keys[i]]);
    if (PyDict_SetItem(pykwargs, k, v)) {
      Py_DECREF(pykwargs);
      return NULL;
    }
    Py_DECREF(k);
    Py_DECREF(v);
  }

  return pykwargs;
}

int jsToPython_Ready() {
  Array = new val(val::global("Array"));
  Object = new val(val::global("Object"));
  return 0;
}
