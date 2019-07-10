/* Experimental spython interpreter for Linux
 *
 * Christian Heimes <christian@python.org>
 *
 * Licensed to PSF under a Contributor Agreement.
 */
#include "Python.h"
#include "pystrhex.h"

/* xattr, stat */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/xattr.h>

/* seccomp */
#include <sys/prctl.h>
#include <seccomp.h>

/* hashing */
#include <openssl/evp.h>

/* logging */
#include <syslog.h>

// 2 MB
#define MAX_PY_FILE_SIZE (2*1024*1024)

#define XATTR_NAME "user.org.python.x-spython-hash"
#define XATTR_LENGTH ((EVP_MAX_MD_SIZE * 2) + 1)

/* Block setxattr syscalls
 */
static int
spython_seccomp_setxattr(int kill) {
    scmp_filter_ctx *ctx = NULL;
    uint32_t action;
    int rc = ENOMEM;
    unsigned int i;
    int syscalls[] = {
        SCMP_SYS(setxattr),
        SCMP_SYS(fsetxattr),
        SCMP_SYS(lsetxattr)
    };

    if (kill) {
       action = SCMP_ACT_KILL_PROCESS;
    } else {
        action = SCMP_ACT_ERRNO(EPERM);
    }
    /* execve(2) does not grant additional privileges */
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == -1) {
        perror("PR_SET_NO_NEW_PRIVS=1\n");
        return -1;
    }
    /* allow all syscalls by default */
    ctx = seccomp_init(SCMP_ACT_ALLOW);
    if (ctx == NULL) {
        goto end;
    }
    /* block setxattr syscalls */
    for (i=0; i < (sizeof(syscalls)/sizeof(syscalls[0])); i++) {
        rc = seccomp_rule_add(ctx, action, syscalls[i], 0);
        if (rc < 0) {
            goto end;
        }
    }
    /* load seccomp rules into Kernel */
    rc = seccomp_load(ctx);
    if (rc < 0) {
        goto end;
    }

  end:
    seccomp_release(ctx);
    if (rc != 0) {
        perror("seccomp failed.\n");
    }
    return rc;
}

/* very file properties */
static int
spython_check_file(const char *filename, int fd)
{
    struct stat sb;
    struct statvfs sbvfs;

    if (fstat(fd, &sb) == -1) {
        PyErr_SetFromErrnoWithFilename(PyExc_OSError, filename);
        return -1;
    }

    /* Only open regular files */
    if (!S_ISREG(sb.st_mode)) {
        errno = EINVAL;
        PyErr_SetFromErrnoWithFilename(PyExc_OSError, filename);
        return -1;
    }

    /* limit file size */
    if (sb.st_size > MAX_PY_FILE_SIZE) {
        errno = EFBIG;
        PyErr_SetFromErrnoWithFilename(PyExc_OSError, filename);
        return -1;
    }

    /* check that mount point is not NOEXEC */
    if (fstatvfs(fd, &sbvfs) == -1) {
        PyErr_SetFromErrnoWithFilename(PyExc_OSError, filename);
        return -1;
    }
    if ((sbvfs.f_flag & ST_NOEXEC) == ST_NOEXEC) {
        errno = EINVAL;
        PyErr_SetFromErrnoWithFilename(PyExc_OSError, filename);
        return -1;
    }
    return 0;
}


/* hash a Python bytes object with OpenSSL */
static PyObject*
spython_hash_bytes(const char *filename, PyObject *buffer)
{
    char *buf;
    Py_ssize_t size = 0;
    const EVP_MD *md = EVP_sha256();
    EVP_MD_CTX *ctx = NULL;
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_size;
    PyObject *result = NULL;

    if (PyBytes_AsStringAndSize(buffer, &buf, &size) == -1) {
        goto end;
    }

    if ((ctx = EVP_MD_CTX_new()) == NULL) {
        PyErr_SetString(PyExc_ValueError, "EVP_MD_CTX_new() failed");
        goto end;
    }
    if (!EVP_DigestInit(ctx, md)) {
        PyErr_SetString(PyExc_ValueError, "EVP_DigestInit SHA-256 failed");
        goto end;
    }
    if (!EVP_DigestUpdate(ctx, (const void*)buf, (unsigned int)size)) {
        PyErr_SetString(PyExc_ValueError, "EVP_DigestUpdate() failed");
        goto end;
    }
    if (!EVP_DigestFinal_ex(ctx, digest, &digest_size)) {
        PyErr_SetString(PyExc_ValueError, "EVP_DigestFinal() failed");
        goto end;
    }
    result = _Py_strhex((const char *)digest, (Py_ssize_t)digest_size);

  end:
    EVP_MD_CTX_free(ctx);
    return result;
}

static PyObject*
spython_fgetxattr(const char *filename, int fd)
{
    char buf[XATTR_LENGTH];
    Py_ssize_t size;

    size = fgetxattr(fd, XATTR_NAME, (void*)buf, sizeof(buf));
    if (size == -1) {
        PyErr_Format(PyExc_OSError, "File %s has no xattr %s.", filename, XATTR_NAME);
        return NULL;
    }
    return PyUnicode_DecodeASCII(buf, size, "strict");
}


static PyObject*
spython_open_stream(const char *filename, int fd)
{
    PyObject *stream = NULL;
    PyObject *iomod = NULL;
    PyObject *fileio = NULL;
    PyObject *buffer = NULL;
    PyObject *res = NULL;
    PyObject *file_hash = NULL;
    PyObject *xattr_hash = NULL;
    int cmp;

    if (spython_check_file(filename, fd) != 0) {
        goto end;
    }

    if ((iomod = PyImport_ImportModule("_io")) == NULL) {
        goto end;
    }

    /* read file with _io module */
    fileio = PyObject_CallMethod(iomod, "FileIO", "isi", fd, "r", 0);
    if (fileio == NULL) {
        goto end;
    }
    buffer = PyObject_CallMethod(fileio, "readall", NULL);
    res = PyObject_CallMethod(fileio, "close", NULL);
    if ((buffer == NULL) || (res == NULL)) {
        goto end;
    }

    if ((file_hash = spython_hash_bytes(filename, buffer)) == NULL) {
        goto end;
    }
    if ((xattr_hash = spython_fgetxattr(filename, fd)) == NULL) {
        goto end;
    }
    cmp = PyObject_RichCompareBool(file_hash, xattr_hash, Py_EQ);
    switch(cmp) {
      case 1:
        stream = PyObject_CallMethod(iomod, "BytesIO", "O", buffer);
        break;
      case 0:
        PyErr_Format(PyExc_ValueError,
                     "File hash mismatch: %s (expected: %R, got %R)",
                      filename, xattr_hash, file_hash);
        goto end;
      default:
        goto end;
    }

  end:
    Py_XDECREF(buffer);
    Py_XDECREF(iomod);
    Py_XDECREF(fileio);
    Py_XDECREF(res);
    Py_XDECREF(file_hash);
    Py_XDECREF(xattr_hash);
    return stream;
}

static PyObject*
spython_open_code(PyObject *path, void *userData)
{
    PyObject *filename_obj = NULL;
    const char *filename;
    int fd = -1;
    PyObject *stream = NULL;

    if (PySys_Audit("spython.open_code", "O", path) < 0) {
        goto end;
    }

    if (!PyUnicode_FSConverter(path, &filename_obj)) {
        goto end;
    }
    filename = PyBytes_AS_STRING(filename_obj);

    fd = _Py_open(filename, O_RDONLY);
    if (fd < 0) {
        goto end;
    }

    stream = spython_open_stream(filename, fd);
    if (stream == NULL) {
        syslog(LOG_CRIT, "spython failed to verify file %s.", filename);
    }

  end:
    Py_XDECREF(filename_obj);
    if (fd >= 0) {
        close(fd);
    }
    return stream;
}

int
main(int argc, char **argv)
{
    PyStatus status;
    PyConfig config;

    /* block syscalls */
    if (spython_seccomp_setxattr(0) < 0) {
        exit(1);
    }

    /* configure syslog */
    openlog(NULL, LOG_CONS | LOG_PERROR | LOG_PID, LOG_USER);

    /* initialize Python in isolated mode, but allow argv */
    status = PyConfig_InitIsolatedConfig(&config);
    if (PyStatus_Exception(status)) {
        goto fail;
    }

    /* install hooks */
    PyFile_SetOpenCodeHook(spython_open_code, NULL);

    /* handle and parse argv */
    config.parse_argv = 1;
    status = PyConfig_SetBytesArgv(&config, argc, argv);
    if (PyStatus_Exception(status)) {
        goto fail;
    }

    /* perform remaining initialization */
    status = PyConfig_Read(&config);
    if (PyStatus_Exception(status)) {
        goto fail;
    }

    status = Py_InitializeFromConfig(&config);
    if (PyStatus_Exception(status)) {
        goto fail;
    }
    PyConfig_Clear(&config);

    return Py_RunMain();

  fail:
    PyConfig_Clear(&config);
    if (PyStatus_IsExit(status)) {
        return status.exitcode;
    }
    /* Display the error message and exit the process with
       non-zero exit code */
    Py_ExitStatusException(status);
}
