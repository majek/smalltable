#include "Python.h"
#include "qlist.h"

static PyObject *ListTooBig;
static PyObject *NotSorted;

/* warn: allocated on stack */
/* +magic +firstitem +<1M items> +stop +9bytes_safety= */
#define MAX_BUF_SIZE (9 +9 +1024*1024 +1 +9)
#define MAX_ITEMS_NUMBER (1024*1024+1)


static PyObject *qlist_pack_array(PyObject *self, PyObject *args) 
{
	char *arr;
	int arr_sz;
	int itemsize;
	if (!PyArg_ParseTuple(args, "s#i", &arr, &arr_sz, &itemsize)) {
		PyErr_Format(PyExc_TypeError, "<string> <itemsize> required");
		return NULL;
	}
	
	if (itemsize != 4 && itemsize != 8) {
		PyErr_Format(PyExc_TypeError, "itemsize must be 4 or 8");
		return NULL;
	}

	if (arr_sz % itemsize) {
		PyErr_Format(PyExc_TypeError,
				"string size must be a multiplication of %i",
				itemsize);
		return NULL;
	}
	int items_sz = arr_sz/itemsize;
	u_int64_t *items;
	
	if(itemsize == 4) {
		items = (u_int64_t*)PyMem_Malloc(sizeof(u_int64_t) * items_sz);
		if (NEVER(items == NULL)) {
			PyErr_NoMemory();
			return NULL;
		}
		int i;
		unsigned int *items32 = (unsigned int *)arr;
		for(i = 0; i < items_sz; i++) {
			items[i] = items32[i];
		}
	} else { // itemsize = 8
		items = (u_int64_t *)arr;
	}
	char qbuf[MAX_BUF_SIZE];
	int r = qlist_pack((u_int8_t*)qbuf, sizeof(qbuf), items, items_sz);
	if(itemsize == 4) {
		PyMem_FREE(items);
	}
	if(r == -1) {
		PyErr_Format(ListTooBig, "supplied data is too big to fit into qlist");
		return NULL;
	}
	if(r == -2) {
		PyErr_Format(NotSorted, "items aren't sorted");
		return NULL;
	}
	
	return PyString_FromStringAndSize(qbuf, r);
}


static PyObject *qlist_unpack_to_array(PyObject *self, PyObject *args)
{	
	PyObject *ret;
	char *qbuf;
	int qbuf_sz;
	int itemsize;
	if (!PyArg_ParseTuple(args, "s#i", &qbuf, &qbuf_sz, &itemsize)) {
		PyErr_Format(PyExc_TypeError, "<string> <itemsize> required");
		return NULL;
	}
	
	if (itemsize != 4 && itemsize != 8) {
		PyErr_Format(PyExc_TypeError, "itemsize must be 4 or 8");
		return NULL;
	}
	if(qbuf_sz < 1) {
		PyErr_Format(PyExc_TypeError, "qbuf must contain some data");
		return NULL;
	}

	u_int64_t *items = (u_int64_t*)PyMem_Malloc(sizeof(u_int64_t) * MAX_ITEMS_NUMBER );
	if (NEVER(items == NULL)) {
		PyErr_NoMemory();
		return NULL;
	}
	int items_sz = MAX_ITEMS_NUMBER;

	int r = qlist_unpack(items, items_sz, (u_int8_t*)qbuf);
	if(r < 0) {
		if(r == -1)
			PyErr_Format(ListTooBig, "supplied qbuf is too big to fit into array");
		if(r == -2)
			PyErr_Format(PyExc_TypeError, "qbuf magic is invalid");
		ret = NULL;
		goto done;
	}

	if(itemsize == 8) {
		ret = PyString_FromStringAndSize((char *)items, sizeof(u_int64_t) * r);
		goto done;
	}

	// itemsize == 4, reuse the same buffer
	int i;
	u_int32_t *items32 = (u_int32_t *)items;
	for(i = 0; i < r; i++) {
		items32[i] = items[i];
	}
	ret = PyString_FromStringAndSize((char *)items, sizeof(u_int32_t) * r);
done:
	PyMem_FREE(items);
	return(ret);
}

#define PREFIX					\
	char *qbufa;				\
	int qbufa_sz;				\
	char *qbufb;				\
	int qbufb_sz;				\
	if (!PyArg_ParseTuple(args, "s#s#", &qbufa, &qbufa_sz, &qbufb, &qbufb_sz)) {	\
		PyErr_Format(PyExc_TypeError, "<string> <string> required");		\
		return NULL;			\
	}					\
	if(qbufa_sz < 1 || qbufb_sz < 1) {	\
		PyErr_Format(PyExc_TypeError, "qbuf must contain some data");	\
		return NULL;			\
	}					\
						\
	char qbufc[MAX_BUF_SIZE];		\
	int r;

#define SUFFIX					\
	if(r == -1) {				\
		PyErr_Format(ListTooBig, "result is too big to fit into qbuf");	\
		return NULL;			\
	}					\
	if(r == -2) {				\
		PyErr_Format(PyExc_TypeError, "qbuf magic is invalid");	\
		return NULL;			\
	}					\
						\
	return PyString_FromStringAndSize(qbufc, r);

static PyObject *qlist_do_or(PyObject *self, PyObject *args)
{
	PREFIX;
	r = qlist_or((u_int8_t*)qbufc, sizeof(qbufc), (u_int8_t*)qbufa, (u_int8_t*)qbufb);
	SUFFIX;
}

static PyObject *qlist_do_and(PyObject *self, PyObject *args)
{
	PREFIX;
	r = qlist_and((u_int8_t*)qbufc, sizeof(qbufc), (u_int8_t*)qbufa, (u_int8_t*)qbufb);
	SUFFIX;
}

static PyObject *qlist_do_andnot(PyObject *self, PyObject *args)
{
	PREFIX;
	r = qlist_andnot((u_int8_t*)qbufc, sizeof(qbufc), (u_int8_t*)qbufa, (u_int8_t*)qbufb);
	SUFFIX;
}



static PyMethodDef Methods[] =
{
	{"pack_array", qlist_pack_array, METH_VARARGS},
	{NULL, NULL}
};

PyMODINIT_FUNC
init_qlist(void)
{
	PyObject *m;
	
	m = Py_InitModule("_qlist", Methods);
	if (m == NULL) // never
		return;
}
