#include <node.h>	/* �K�{ */
#include <uv.h>		/* �񓯊��R�[���o�b�N�̈� */
#include <thread>	/* for std::this_thread::sleep_for() */
using namespace v8;	/* �K�{ */

bool _bStop = false;
bool _bExec = false;

/* �X���b�h�ԏ�� */
typedef struct my_struct
{
	int request;					/* �v��(�����ҋ@�b) */
	int result;						/* ����(�ҋ@����ms) */
	Persistent<Function> callback;	/* �񓯊����������R�[���o�b�N�֐� */
};

void _stopping(uv_work_t* req) {
	my_struct* data = static_cast<my_struct*>(req->data);	/* �������p�� */
	while (_bExec)	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		data->request++;
	}
}

void _stoped(uv_work_t* req, int status) {

	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	my_struct* data = static_cast<my_struct*>(req->data);	/* �������p�� */
	Local<Value> argv[1] = { Number::New(isolate, data->result) };		/* �R�[���o�b�N�p�����ݒ� */
	Local<Function> cb = Local<Function>::New(isolate, data->callback);	/* �R�[���o�b�N�֐��ݒ� */
	cb->Call(isolate->GetCurrentContext()->Global(), 1, argv);			/* �R�[���o�b�N */
	data->callback.Reset();
	/* �㏈�� */
	delete data;
	delete req;
}

void _execute(uv_work_t* req) {
	my_struct* data = static_cast<my_struct*>(req->data);	/* �������p�� */
	data->result = data->request;
	std::this_thread::sleep_for(std::chrono::milliseconds(data->request));
}

void _complete(uv_work_t* req, int ) {

	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	my_struct* data = static_cast<my_struct*>(req->data);	/* �������p�� */
	Local<Value> argv[1] = { Number::New(isolate, data->result) };		/* �R�[���o�b�N�p�����ݒ� */
	Local<Function> cb = Local<Function>::New(isolate, data->callback);	/* �R�[���o�b�N�֐��ݒ� */
	cb->Call(isolate->GetCurrentContext()->Global(), 1, argv);			/* �R�[���o�b�N */


	if (_bStop){
		/* �㏈�� */
		data->callback.Reset();
		delete data; data = NULL;
		delete req; req = NULL;
		_bExec = false;	/* ���s�t���O���� */
		return;
	}
	uv_queue_work(uv_default_loop(), req, _execute, _complete);	/* �J��Ԃ� */
}

//--------------------------------------------------------------------------------
//[�O��]�^�C�}�[�J�n�v��
//[����]args[0]:	��~�����R�[���o�b�N�֐� */
//		args[1]:	�C���^�[�o��(ms)
//[�ߒl] 0:�J�n�v����t,-1:�^�C�}�[���s��*/
//--------------------------------------------------------------------------------
void AsyncStart(const FunctionCallbackInfo<Value>& args) {

	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	/* �����`�F�b�N(�����̐�) */
	if (args.Length() != 2){
		isolate->ThrowException(Exception::TypeError(
			String::NewFromUtf8(isolate, "Wrong number of arguments")));
		return;
	}
	/* �����`�F�b�N(�����̌^) */
	if (!args[0]->IsFunction() || !args[1]->IsNumber()) {
		isolate->ThrowException(Exception::TypeError(
			String::NewFromUtf8(isolate, "Wrong arguments")));
		return;
	}
	if (_bExec){
		args.GetReturnValue().Set(-1);
	}
	else{
		/* �����̏����L�� */
		my_struct* data = new my_struct;
		data->callback.Reset(isolate, args[0].As<Function>());
		data->request = args[1]->Int32Value();
		data->result = 0;
		_bExec = true;
		_bStop = false;

		/* �o�b�N�O���E���h�Ŏ��s */
		uv_work_t *req = new uv_work_t;
		req->data = data;
		uv_queue_work(uv_default_loop(), req, _execute, _complete);
		args.GetReturnValue().Set(0);
	}
}
//--------------------------------------------------------------------------------
//[�O��]�^�C�}�[��~�v��
//[����]args[0]:	��~�����R�[���o�b�N�֐� */
//[�ߒl] 0:��~�v����t,1:�����s���̒�~�v����t*/
//--------------------------------------------------------------------------------
void AsyncStop(const FunctionCallbackInfo<Value>& args) {

	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);

	/* �����`�F�b�N(�����̐�) */
	if (args.Length() != 1){
		isolate->ThrowException(Exception::TypeError(
			String::NewFromUtf8(isolate, "Wrong number of arguments")));
		return;
	}
	/* �����s�Ȃ炷���ɃR�[���o�b�N��Ԃ� */
	if (!_bExec){
		Local<Function> func = args[0].As<Function>();
		Local<Value> argv[1] = { Number::New(isolate, 0) };
		func->Call(isolate->GetCurrentContext()->Global(), 1, argv);
		args.GetReturnValue().Set(1);
	}
	else{
		/* �����̏����L�� */
		my_struct* data = new my_struct;
		data->callback.Reset(isolate, args[0].As<Function>());
		data->result = 0;

		_bStop = true;	/* ��~�v�� */

		/* �o�b�N�O���E���h�Ŏ��s */
		uv_work_t *req = new uv_work_t;
		req->data = data;
		uv_queue_work(uv_default_loop(), req, _stopping, _stoped);
		args.GetReturnValue().Set(0);
	}
}

/* �����ɊO������Ă΂��֐��������Ă��� */
/* �O������Ă΂�閼�O�Ɠ����̊֐����̂Ђ��t�� */
void init(Handle<Object> exports) {
	NODE_SET_METHOD(exports, "start", AsyncStart);
	NODE_SET_METHOD(exports, "stop", AsyncStop);
}

/* ���W���[����require����鎞�ɌĂ΂�� */
NODE_MODULE(asyncWait, init)