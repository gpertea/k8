#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <zlib.h>
#include <v8.h>

#define SAVE_PTR(_args, _index, _ptr)  (_args).This()->SetAlignedPointerInInternalField(_index, (void*)(_ptr))
#define LOAD_PTR(_args, _index, _type) reinterpret_cast<_type>((_args).This()->GetAlignedPointerFromInternalField(_index))
#define SAVE_VALUE(_args, _index, _val) (_args).This()->SetInternalField(_index, _val)
#define LOAD_VALUE(_args, _index) (_args).This()->GetInternalField(_index)

#define JS_STR(...) v8::String::New(__VA_ARGS__)

#define JS_THROW(type, reason) v8::ThrowException(v8::Exception::type(JS_STR(reason)))
#define JS_ERROR(reason) JS_THROW(Error, reason)
#define JS_METHOD(_func, _args) v8::Handle<v8::Value> _func(const v8::Arguments &(_args))

#define ASSERT_CONSTRUCTOR(_args) if (!(_args).IsConstructCall()) { return JS_ERROR("Invalid call format. Please use the 'new' operator."); }

/*******************************
 *** Fundamental V8 routines ***
 *******************************/

const char *k8_cstr(const v8::String::AsciiValue &str)
{
	return *str? *str : "<N/A>";
}

static void k8_exception(v8::TryCatch *try_catch)
{
	v8::HandleScope handle_scope;
	v8::String::AsciiValue exception(try_catch->Exception());
	const char* exception_string = k8_cstr(exception);
	v8::Handle<v8::Message> message = try_catch->Message();
	if (message.IsEmpty()) {
		// V8 didn't provide any extra information about this error; just print the exception.
		fprintf(stderr, "%s\n", exception_string);
	} else {
		// Print (filename):(line number): (message).
		v8::String::AsciiValue filename(message->GetScriptResourceName());
		const char* filename_string = k8_cstr(filename);
		int linenum = message->GetLineNumber();
		fprintf(stderr, "%s:%i: %s\n", filename_string, linenum, exception_string);
		// Print line of source code.
		v8::String::AsciiValue sourceline(message->GetSourceLine());
		const char *sourceline_string = k8_cstr(sourceline);
		fprintf(stderr, "%s\n", sourceline_string);
		// Print wavy underline (GetUnderline is deprecated).
		int start = message->GetStartColumn();
		for (int i = 0; i < start; i++) fputc(' ', stderr);
		int end = message->GetEndColumn();
		for (int i = start; i < end; i++) fputc('^', stderr);
		fputc('\n', stderr);
		v8::String::AsciiValue stack_trace(try_catch->StackTrace());
		if (stack_trace.length() > 0) { // TODO: is the following redundant?
			const char* stack_trace_string = k8_cstr(stack_trace);
			fputs(stack_trace_string, stderr); fputc('\n', stderr);
		}
	}
}

bool k8_execute(v8::Handle<v8::String> source, v8::Handle<v8::Value> name, bool prt_rst)
{
	v8::HandleScope handle_scope;
	v8::TryCatch try_catch;
	v8::Handle<v8::Script> script = v8::Script::Compile(source, name);
	if (script.IsEmpty()) {
		k8_exception(&try_catch);
		return false;
	} else {
		v8::Handle<v8::Value> result = script->Run();
		if (result.IsEmpty()) {
			assert(try_catch.HasCaught());
			k8_exception(&try_catch);
			return false;
		} else {
			assert(!try_catch.HasCaught());
			if (prt_rst && !result->IsUndefined()) {
				v8::String::AsciiValue str(result);
				puts(k8_cstr(str));
			}
			return true;
		}
	}
}

v8::Handle<v8::String> k8_readfile(const char *name)
{
	FILE* file = fopen(name, "rb");
	if (file == NULL) return v8::Handle<v8::String>();

	fseek(file, 0, SEEK_END);
	int size = ftell(file);
	rewind(file);

	char* chars = new char[size + 1];
	chars[size] = '\0';
	for (int i = 0; i < size;) {
		int read = static_cast<int>(fread(&chars[i], 1, size - i, file));
		i += read;
	}
	fclose(file);
	v8::Handle<v8::String> result = v8::String::New(chars, size);
	delete[] chars;
	return result;
}

/******************************
 *** New built-in functions ***
 ******************************/

JS_METHOD(k8_print, args)
{
	for (int i = 0; i < args.Length(); i++) {
		v8::HandleScope handle_scope;
		if (i) putchar('\t');
		v8::String::AsciiValue str(args[i]);
		fputs(k8_cstr(str), stdout);
	}
	putchar('\n');
	return v8::Undefined();
}

JS_METHOD(k8_exit, args)
{
	int exit_code = args[0]->Int32Value();
	fflush(stdout); fflush(stderr);
	exit(exit_code);
	return v8::Undefined();
}

JS_METHOD(k8_version, args)
{
	return v8::String::New(v8::V8::GetVersion());
}

JS_METHOD(k8_load, args)
{
	char buf[1024], *path = getenv("K8_LIBRARY_PATH");
	FILE *fp;
	for (int i = 0; i < args.Length(); i++) {
		v8::HandleScope handle_scope;
		v8::String::Utf8Value file(args[i]);
		buf[0] = 0;
		if ((fp = fopen(*file, "r")) != 0) {
			fclose(fp);
			strcpy(buf, *file);
		} else if (path) { // TODO: to allow multiple paths separated by ":"
			strcpy(buf, path); strcat(buf, "/"); strcat(buf, *file);
			if ((fp = fopen(*file, "r")) == 0) buf[0] = 0;
			else fclose(fp);
		}
		if (buf[0] == 0) return JS_THROW(Error, "[load] fail to locate the file");
		v8::Handle<v8::String> source = k8_readfile(buf);
		if (!k8_execute(source, v8::String::New(*file), false))
			return JS_THROW(Error, "[load] fail to execute the file");
	}
	return v8::Undefined();
}

/*******************
 *** File object ***
 *******************/

JS_METHOD(k8_file, args)
{
	v8::HandleScope scope;
	ASSERT_CONSTRUCTOR(args);
	FILE *fpw = 0;
	gzFile fpr = 0;
	if (args.Length()) {
		SAVE_VALUE(args, 0, args[0]);
		v8::String::AsciiValue file(args[0]);
		if (args.Length() >= 2) {
			SAVE_VALUE(args, 1, args[1]);
			v8::String::AsciiValue mode(args[1]);
			const char *cstr = k8_cstr(mode);
			if (strchr(cstr, 'w')) fpw = fopen(k8_cstr(file), cstr);
			else fpr = gzopen(k8_cstr(file), cstr);
		} else {
			SAVE_VALUE(args, 1, JS_STR("r"));
			fpr = gzopen(*file, "r");
		}
		if (fpr == 0 && fpw == 0)
			return JS_THROW(Error, "[File] Fail to open the file");
	} else {
		SAVE_VALUE(args, 0, JS_STR("-"));
		SAVE_VALUE(args, 1, JS_STR("r"));
		fpr = gzdopen(fileno(stdin), "r");
	}
	SAVE_PTR(args, 2, fpr);
	SAVE_PTR(args, 3, fpw);
	return args.This();
}

JS_METHOD(k8_file_close, args)
{
	gzFile fpr = LOAD_PTR(args, 2, gzFile);
	FILE  *fpw = LOAD_PTR(args, 3, FILE*);
	if (fpr) gzclose(fpr);
	if (fpw) fclose(fpw);
	SAVE_PTR(args, 2, 0);
	SAVE_PTR(args, 3, 0);
	return v8::Undefined();
}

JS_METHOD(k8_file_read, args)
{
	v8::HandleScope scope;
	gzFile fp = LOAD_PTR(args, 2, gzFile);
	if (args.Length() == 0) return v8::Null();
	long len, rdlen;
	v8::String::AsciiValue clen(args[0]);
	len = atol(k8_cstr(clen));
	char buf[len];
	rdlen = gzread(fp, buf, len);
	return scope.Close(v8::String::New(buf, rdlen));
}

JS_METHOD(k8_file_write, args)
{
	v8::HandleScope scope;
	FILE *fp = LOAD_PTR(args, 3, FILE*);
	if (args.Length() == 0) return scope.Close(v8::Integer::New(0));
	v8::String::AsciiValue vbuf(args[0]);
	long len = fwrite(*vbuf, 1, vbuf.length(), fp);
	return scope.Close(v8::Integer::New(len));
}

/**********************
 *** iStream object ***
 **********************/

static long obj_read(v8::Handle<v8::Object> &obj, void *buf, long len)
{
	v8::HandleScope scope;
	v8::Handle<v8::Function> func = obj->Get(v8::String::New("read")).As<v8::Function>();
	v8::Handle<v8::Value> arg = v8::Integer::New(len);
	v8::Handle<v8::Value> rst = func->Call(obj, 1, &arg);
	v8::String::AsciiValue vbuf(rst);
	long ret = vbuf.length();
	memcpy(buf, *vbuf, ret);
	return ret;
}

#define KS_SEP_SPACE 0 // isspace(): \t, \n, \v, \f, \r
#define KS_SEP_TAB   1 // isspace() && !' '
#define KS_SEP_LINE  2 // line separator: " \n" (Unix) or "\r\n" (Windows)
#define KS_SEP_MAX   2

#define kroundup32(x) (--(x), (x)|=(x)>>1, (x)|=(x)>>2, (x)|=(x)>>4, (x)|=(x)>>8, (x)|=(x)>>16, ++(x))

typedef struct __kstream_t {
	unsigned char *buf;
	int begin, end, is_eof, buf_size;
	struct { long l, m; char *s; } s;
} kstream_t;

#define ks_eof(ks) ((ks)->is_eof && (ks)->begin >= (ks)->end)
#define ks_rewind(ks) ((ks)->is_eof = (ks)->begin = (ks)->end = 0)

static inline kstream_t *ks_init(int __bufsize)
{
	kstream_t *ks = (kstream_t*)calloc(1, sizeof(kstream_t));
	ks->buf_size = __bufsize;
	ks->buf = (unsigned char*)malloc(__bufsize);
	return ks;
}

static inline void ks_destroy(kstream_t *ks)
{ 
	if (ks) { free(ks->buf); free(ks->s.s); free(ks); }
}

static int ks_getuntil(v8::Handle<v8::Object> &fp, kstream_t *ks, int delimiter, int *dret)
{
	if (dret) *dret = 0;
	ks->s.l = 0;
	if (ks->begin >= ks->end && ks->is_eof) return -1;
	for (;;) {
		int i;
		if (ks->begin >= ks->end) {
			if (!ks->is_eof) {
				ks->begin = 0;
				ks->end = obj_read(fp, ks->buf, ks->buf_size);
				if (ks->end < ks->buf_size) ks->is_eof = 1;
				if (ks->end == 0) break;
			} else break;
		}
		if (delimiter == KS_SEP_LINE) {
			for (i = ks->begin; i < ks->end; ++i)
				if (ks->buf[i] == '\n') break;
		} else if (delimiter > KS_SEP_MAX) {
			for (i = ks->begin; i < ks->end; ++i)
				if (ks->buf[i] == delimiter) break;
		} else if (delimiter == KS_SEP_SPACE) {
			for (i = ks->begin; i < ks->end; ++i)
				if (isspace(ks->buf[i])) break;
		} else if (delimiter == KS_SEP_TAB) {
			for (i = ks->begin; i < ks->end; ++i)
				if (isspace(ks->buf[i]) && ks->buf[i] != ' ') break;
		} else i = 0; /* never come to here! */
		if (ks->s.m - ks->s.l < i - ks->begin + 1) {
			ks->s.m = ks->s.l + (i - ks->begin) + 1;
			kroundup32(ks->s.m);
			ks->s.s = (char*)realloc(ks->s.s, ks->s.m);
		}
		memcpy(ks->s.s + ks->s.l, ks->buf + ks->begin, i - ks->begin);
		ks->s.l = ks->s.l + (i - ks->begin);
		ks->begin = i + 1;
		if (i < ks->end) {
			if (dret) *dret = ks->buf[i];
			break;
		}
	}
	if (ks->s.s == 0) {
		ks->s.m = 1;
		ks->s.s = (char*)calloc(1, 1);
	} else if (delimiter == KS_SEP_LINE && ks->s.l > 1 && ks->s.s[ks->s.l-1] == '\r') --ks->s.l;
	ks->s.s[ks->s.l] = '\0';
	return ks->s.l;
}

JS_METHOD(k8_istream, args)
{
	kstream_t *ks;
	ASSERT_CONSTRUCTOR(args);
	if (args.Length() == 0) return v8::Null();
	ks = ks_init(65536);
	SAVE_VALUE(args, 0, args[0]);
	SAVE_PTR(args, 1, ks);
	return args.This();
}

JS_METHOD(k8_istream_readline, args)
{
	v8::HandleScope scope;
	v8::Handle<v8::Object> obj = LOAD_VALUE(args, 0)->ToObject();
	kstream_t *ks = LOAD_PTR(args, 1, kstream_t*);
	int dret, ret, sep = KS_SEP_LINE;
	if (args.Length()) {
		if (args[0]->IsString()) {
			v8::HandleScope scope2;
			v8::String::AsciiValue str(args[0]);
			sep = int(k8_cstr(str)[0]);
		} else if (args[0]->IsInt32())
			sep = args[0]->Int32Value();
	}
	ret = ks_getuntil(obj, ks, sep, &dret);
	if (ret >= 0) return scope.Close(v8::String::New(ks->s.s, ks->s.l));
	return v8::Null();
}

JS_METHOD(k8_istream_close, args)
{
	v8::HandleScope scope;
	v8::Handle<v8::Object> obj = LOAD_VALUE(args, 0)->ToObject();
	kstream_t *ks = LOAD_PTR(args, 1, kstream_t*);
	ks_destroy(ks);
	v8::Handle<v8::Value> func = obj->Get(v8::String::New("close"));
	if (func->IsFunction())
		func.As<v8::Function>()->Call(obj, 0, 0);
	SAVE_VALUE(args, 0, v8::Undefined());
	SAVE_PTR(args, 1, 0);
	return v8::Undefined();
}

/*********************
 *** Main function ***
 *********************/

static v8::Persistent<v8::Context> CreateShellContext() // adapted from shell.cc
{
	v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();
	global->Set(v8::String::New("print"), v8::FunctionTemplate::New(k8_print));
	global->Set(v8::String::New("exit"), v8::FunctionTemplate::New(k8_exit));
	global->Set(v8::String::New("load"), v8::FunctionTemplate::New(k8_load));
	global->Set(v8::String::New("version"), v8::FunctionTemplate::New(k8_version));
	{
		v8::HandleScope scope;
		v8::Handle<v8::FunctionTemplate> ft = v8::FunctionTemplate::New(k8_file);
		ft->SetClassName(JS_STR("File"));
		ft->InstanceTemplate()->SetInternalFieldCount(4); // (fn, mode, fpr, fpw)
		v8::Handle<v8::ObjectTemplate> pt = ft->PrototypeTemplate();
		pt->Set("read", v8::FunctionTemplate::New(k8_file_read));
		pt->Set("write", v8::FunctionTemplate::New(k8_file_write));
		pt->Set("close", v8::FunctionTemplate::New(k8_file_close));
		global->Set(v8::String::New("File"), ft);	
	}
	{
		v8::HandleScope scope;
		v8::Handle<v8::FunctionTemplate> ft = v8::FunctionTemplate::New(k8_istream);
		ft->SetClassName(JS_STR("iStream"));
		ft->InstanceTemplate()->SetInternalFieldCount(2);
		v8::Handle<v8::ObjectTemplate> pt = ft->PrototypeTemplate();
		pt->Set("readline", v8::FunctionTemplate::New(k8_istream_readline));
		pt->Set("close", v8::FunctionTemplate::New(k8_istream_close));
		global->Set(v8::String::New("iStream"), ft);	
	}
	return v8::Context::New(NULL, global);
}

static int RunMain(int argc, char* argv[])
{
	for (int i = 1; i < argc; i++) {
		const char* str = argv[i];
		if ((strcmp(str, "-e") == 0 || strcmp(str, "-E") == 0) && i + 1 < argc) {
			v8::Handle<v8::String> file_name = v8::String::New("unnamed");
			v8::Handle<v8::String> source = v8::String::New(argv[++i]);
			if (!k8_execute(source, file_name, (strcmp(str, "-E") == 0))) return 1;
		} else {
			v8::Handle<v8::String> file_name = v8::String::New(str);
			v8::Handle<v8::String> source = k8_readfile(str);
			if (source.IsEmpty()) {
				fprintf(stderr, "Error reading '%s'\n", str);
				continue;
			}
			if (!k8_execute(source, file_name, false)) return 1;
		}
	}
	return 0;
}

int main(int argc, char* argv[])
{
	v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
	int ret;
	if (argc == 1) {
		fprintf(stderr, "Usage: k8 [-e jsCode] [-E jsCode] <src.js>\n");
		return 1;
	}
	{
		v8::HandleScope handle_scope;
		v8::Persistent<v8::Context> context = CreateShellContext();
		if (context.IsEmpty()) {
			fprintf(stderr, "Error creating context\n");
			return 1;
		}
		context->Enter();
		ret = RunMain(argc, argv);
		context->Exit();
		context.Dispose();
	}
	v8::V8::Dispose();
	return ret;
}
