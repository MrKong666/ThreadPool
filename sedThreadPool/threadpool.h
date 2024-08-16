#ifndef THREADPOOL_H
#define THREADPOOL_H
#include<queue>
#include<memory>
#include<mutex>
#include<condition_variable>
#include<atomic>
#include<vector>
#include<thread>
#include<functional>
#include<iostream>
#include<chrono>
#include<unordered_map>
using namespace std;
 

 
//ʵ��һ���ź�����
class Semaphore {

public:
	Semaphore(int limit=0):
	resLimit_(limit),
	isExit(false)
	{}
	~Semaphore() {
		isExit=true;
	}
	//��ȡһ���ź�����Դ
	void wait() {
		if(isExit)return;
		unique_lock<mutex>lock(mtx_);
		cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
		resLimit_--;
	}
	void post() {
		if(isExit)return;
		unique_lock<mutex>lock(mtx_);
		resLimit_++;
		cond_.notify_all();
	}
private:
	atomic_int resLimit_;
	mutex mtx_;
	condition_variable cond_;
	atomic_bool isExit;
};


//Any���ͣ����Խ����������ݵ�����
class Any {
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;
	//������Any���ͽ�������������Դ
	template<typename T>
	Any(T data) :base_(make_unique<Derive<T>>(data)) {

	}

	//��Any������洢��data������ȡ����
	template<typename T>
	T cast_() {
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr) {
			throw "����ת������";
		}
		return pd->data_;
	}
private:
	//�������� 
	class Base {
	public:
		virtual ~Base() = default;
	};
	//����
	template<typename T>
	class Derive:public Base {
	public:
		Derive(T data) :data_(data){};
		T data_;
	};
private:
	unique_ptr<Base>base_;
};




//�̳߳�֧�ֵ�ģʽ
enum class PoolMode
{
	MODE_FIXED,//�̶������߳�
	MODE_CACHED,//�߳������ɶ�̬����
};

//�߳�����
class Thread {
public:
	using ThreadFunc = function<void(int)>;
	//�̹߳���
	Thread(ThreadFunc func);
	//�߳�����
	~Thread();


	//�����߳�
	void start();

	//��ȡ�߳�id
	int getId()const;
private:
	ThreadFunc func_;
	static int gengeratid_;
	int threadId_;//�����߳�id
};

class Task;
//ʵ�ֽ���task������ɺ�ķ���ֵresult
class Result {
public:
	Result(shared_ptr<Task>task, bool isValid = true);
	~Result() = default;
	void setVal(Any any);
	Any get();
private:
	Any any_;//�洢����ֵ
	Semaphore sem_;
	shared_ptr<Task>task_;
	atomic_bool isValid_;
	
};
//����������
class Task {
public:
	void exec();
	void setResult(Result* res);
	//�û��ɼ̳д����Զ����̷߳���
	virtual Any run() = 0;
	Result* result_;
};
//�̳߳�����
class ThreadPool {
public:
	ThreadPool();
	~ThreadPool();
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool operator=(const ThreadPool&) = delete;
	//����̳߳ع���ģʽ
	void setMode(PoolMode mode);


	//����task�������������ֵ
	void settaskQueMaxThreshHold(int threshhold);
	

	
	//�����߳�����������ֵcachedģʽ��
	void setthreadSizeThreshHold(int threadhold);
	//���̳߳��ύ����
	Result submitTask(shared_ptr<Task>sp);

	//�����̳߳�
	void start(int initThreadSize = thread::hardware_concurrency());
private:
	//��������״̬��鷽��
	bool checkRunningState()const;
	//�����̺߳���
	void threadFunc(int threadid);
private:
	//vector< unique_ptr<Thread >> threads_;
	unordered_map<int,unique_ptr<Thread>>threads_;//�߳��б�
	int initThreadSize_;//��ʼ���߳�����
	atomic_int curThreadSize_;//��ǰ�̳߳����̵߳�����
	atomic_int idleThreadSize_;//��¼���е��߳�����
	int threadSizeThreshHold_;//�߳�����������ֵ

	queue<shared_ptr<Task>>taseQue_;//�������
	atomic_int tasksize_;//���������
	int taskQueMaxThreshHold_;//�������������ֵ

	mutex taskQueMtx_; //����֤��������̰߳�ȫ
	condition_variable notFull_;//�������δ��
	condition_variable notEmpty_;//������в���
	condition_variable exitcond_;//�˳���������

	PoolMode poolMode_;//��ǰ�̳߳صĹ���ģʽ
	atomic_bool isPoolRunning;//�̳߳ص�����״̬
};
#endif