#include <node.h>	/* 必須 */
#include <uv.h>		/* 非同期コールバックの為 */
#include <thread>	/* for std::this_thread::sleep_for() */
using namespace v8;	/* 必須 */

bool _bStop = false;
bool _bExec = false;

/* スレッド間情報 */
typedef struct my_struct
{
	int request;					/* 要求(処理待機秒) */
	int result;						/* 結果(待機したms) */
	Persistent<Function> callback;	/* 非同期処理完了コールバック関数 */
};

void _stopping(uv_work_t* req) {
	my_struct* data = static_cast<my_struct*>(req->data);	/* 情報引き継ぎ */
	while (_bExec)	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		data->request++;
	}
}

void _stoped(uv_work_t* req, int status) {

	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	my_struct* data = static_cast<my_struct*>(req->data);	/* 情報引き継ぎ */
	Local<Value> argv[1] = { Number::New(isolate, data->result) };		/* コールバック用引数設定 */
	Local<Function> cb = Local<Function>::New(isolate, data->callback);	/* コールバック関数設定 */
	cb->Call(isolate->GetCurrentContext()->Global(), 1, argv);			/* コールバック */
	data->callback.Reset();
	/* 後処理 */
	delete data;
	delete req;
}

void _execute(uv_work_t* req) {
	my_struct* data = static_cast<my_struct*>(req->data);	/* 情報引き継ぎ */
	data->result = data->request;
	std::this_thread::sleep_for(std::chrono::milliseconds(data->request));
}

void _complete(uv_work_t* req, int ) {

	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	my_struct* data = static_cast<my_struct*>(req->data);	/* 情報引き継ぎ */
	Local<Value> argv[1] = { Number::New(isolate, data->result) };		/* コールバック用引数設定 */
	Local<Function> cb = Local<Function>::New(isolate, data->callback);	/* コールバック関数設定 */
	cb->Call(isolate->GetCurrentContext()->Global(), 1, argv);			/* コールバック */


	if (_bStop){
		/* 後処理 */
		data->callback.Reset();
		delete data; data = NULL;
		delete req; req = NULL;
		_bExec = false;	/* 実行フラグ解除 */
		return;
	}
	uv_queue_work(uv_default_loop(), req, _execute, _complete);	/* 繰り返し */
}

//--------------------------------------------------------------------------------
//[外部]タイマー開始要求
//[引数]args[0]:	停止完了コールバック関数 */
//		args[1]:	インターバル(ms)
//[戻値] 0:開始要求受付,-1:タイマー実行中*/
//--------------------------------------------------------------------------------
void AsyncStart(const FunctionCallbackInfo<Value>& args) {

	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	/* 引数チェック(引数の数) */
	if (args.Length() != 2){
		isolate->ThrowException(Exception::TypeError(
			String::NewFromUtf8(isolate, "Wrong number of arguments")));
		return;
	}
	/* 引数チェック(引数の型) */
	if (!args[0]->IsFunction() || !args[1]->IsNumber()) {
		isolate->ThrowException(Exception::TypeError(
			String::NewFromUtf8(isolate, "Wrong arguments")));
		return;
	}
	if (_bExec){
		args.GetReturnValue().Set(-1);
	}
	else{
		/* 引数の情報を記憶 */
		my_struct* data = new my_struct;
		data->callback.Reset(isolate, args[0].As<Function>());
		data->request = args[1]->Int32Value();
		data->result = 0;
		_bExec = true;
		_bStop = false;

		/* バックグラウンドで実行 */
		uv_work_t *req = new uv_work_t;
		req->data = data;
		uv_queue_work(uv_default_loop(), req, _execute, _complete);
		args.GetReturnValue().Set(0);
	}
}
//--------------------------------------------------------------------------------
//[外部]タイマー停止要求
//[引数]args[0]:	停止完了コールバック関数 */
//[戻値] 0:停止要求受付,1:未実行時の停止要求受付*/
//--------------------------------------------------------------------------------
void AsyncStop(const FunctionCallbackInfo<Value>& args) {

	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	/* 引数チェック(引数の数) */
	if (args.Length() != 1){
		isolate->ThrowException(Exception::TypeError(
			String::NewFromUtf8(isolate, "Wrong number of arguments")));
		return;
	}
	/* 未実行ならすぐにコールバックを返す */
	if (!_bExec){
		Local<Function> func = args[0].As<Function>();
		Local<Value> argv[1] = { Number::New(isolate, 0) };
		func->Call(isolate->GetCurrentContext()->Global(), 1, argv);
		args.GetReturnValue().Set(1);
	}
	else{
		/* 引数の情報を記憶 */
		my_struct* data = new my_struct;
		data->callback.Reset(isolate, args[0].As<Function>());
		data->result = 0;

		_bStop = true;	/* 停止要求 */

		/* バックグラウンドで実行 */
		uv_work_t *req = new uv_work_t;
		req->data = data;
		uv_queue_work(uv_default_loop(), req, _stopping, _stoped);
		args.GetReturnValue().Set(0);
	}
}

/* ここに外部から呼ばれる関数を書いていく */
/* 外部から呼ばれる名前と内部の関数名のひも付け */
void init(Handle<Object> exports) {
	NODE_SET_METHOD(exports, "start", AsyncStart);
	NODE_SET_METHOD(exports, "stop", AsyncStop);
}

/* モジュールがrequireされる時に呼ばれる */
NODE_MODULE(asyncWait, init)