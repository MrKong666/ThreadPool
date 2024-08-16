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
 
//实现一个信号量类
class Semaphore {

public:
	Semaphore(int limit=0):resLimit_(limit){}
	~Semaphore() = default;
	//获取一个信号量资源
	void wait() {
		unique_lock<mutex>lock(mtx_);
		cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
		resLimit_--;
	}
	void post() {
		unique_lock<mutex>lock(mtx_);
		resLimit_++;
		cond_.notify_all();
	}
private:
	atomic_int resLimit_;
	mutex mtx_;
	condition_variable cond_;
};


//Any类型，可以接受任意数据的类型
class Any {
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;
	//构造让Any类型接受任意其他资源
	template<typename T>
	Any(T data) :base_(make_unique<Derive<T>>(data)) {

	}

	//把Any对象里存储的data数据提取出来
	template<typename T>
	T cast_() {
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr) {
			throw "类型转换错误";
		}
		return pd->data_;
	}
private:
	//基类类型 
	class Base {
	public:
		virtual ~Base() = default;
	};
	//子类
	template<typename T>
	class Derive:public Base {
	public:
		Derive(T data) :data_(data){};
		T data_;
	};
private:
	unique_ptr<Base>base_;
};




//线程池支持的模式
enum class PoolMode
{
	MODE_FIXED,//固定数量线程
	MODE_CACHED,//线程数量可动态增长
};

//线程类型
class Thread {
public:
	using ThreadFunc = function<void(int)>;
	//线程构造
	Thread(ThreadFunc func);
	//线程析构
	~Thread();


	//启动线程
	void start();

	//获取线程id
	int getId()const;
private:
	ThreadFunc func_;
	static int gengeratid_;
	int threadId_;//保存线程id
};

class Task;
//实现接受task任务完成后的返回值result
class Result {
public:
	Result(shared_ptr<Task>task, bool isValid = true);
	~Result() = default;
	void setVal(Any any);
	Any get();
private:
	Any any_;//存储返回值
	Semaphore sem_;
	shared_ptr<Task>task_;
	atomic_bool isValid_;
};
//任务抽象基类
class Task {
public:
	void exec();
	void setResult(Result* res);
	//用户可继承此类自定义线程方法
	virtual Any run() = 0;
	Result* result_;
};
//线程池类型
class ThreadPool {
public:
	ThreadPool();
	~ThreadPool();
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool operator=(const ThreadPool&) = delete;
	//设计线程池工作模式
	void setMode(PoolMode mode);


	//设置task任务队列上线阈值
	void settaskQueMaxThreshHold(int threshhold);
	

	
	//设置线程数量上限阈值cached模式下
	void setthreadSizeThreshHold(int threadhold);
	//给线程池提交任务
	Result submitTask(shared_ptr<Task>sp);

	//开启线程池
	void start(int initThreadSize = thread::hardware_concurrency());
private:
	//定义运行状态检查方法
	bool checkRunningState()const;
	//定义线程函数
	void threadFunc(int threadid);
private:
	//vector< unique_ptr<Thread >> threads_;
	unordered_map<int,unique_ptr<Thread>>threads_;//线程列表
	int initThreadSize_;//初始的线程数量
	atomic_int curThreadSize_;//当前线程池里线程的数量
	atomic_int idleThreadSize_;//记录空闲的线程数量
	int threadSizeThreshHold_;//线程数量上限阈值

	queue<shared_ptr<Task>>taseQue_;//任务队列
	atomic_int tasksize_;//任务的数量
	int taskQueMaxThreshHold_;//任务队列上限阈值

	mutex taskQueMtx_; //锁保证任务队列线程安全
	condition_variable notFull_;//任务队列未满
	condition_variable notEmpty_;//任务队列不空
	condition_variable exitcond_;//退出条件变量

	PoolMode poolMode_;//当前线程池的工作模式
	atomic_bool isPoolRunning;//线程池的启动状态
};
#endif