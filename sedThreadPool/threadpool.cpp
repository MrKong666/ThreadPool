#include"threadpool.h"

const int TASK_MAX_THRESHHOLE = 1024;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 10;//单位秒
ThreadPool::ThreadPool()
	:initThreadSize_(4),
	tasksize_(0),
	curThreadSize_(0),
	idleThreadSize_(0),
	threadSizeThreshHold_(THREAD_MAX_THRESHHOLD),
	taskQueMaxThreshHold_(TASK_MAX_THRESHHOLE),
	poolMode_(PoolMode::MODE_FIXED),
	isPoolRunning(false)
{}

ThreadPool::~ThreadPool() {
	isPoolRunning = false;
	
	unique_lock<mutex>lock(taskQueMtx_);//在锁内唤醒会解决死锁问题
	notEmpty_.notify_all();//把所有线程唤醒争锁
	//等待所有线程退出
	exitcond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}

//设计线程池工作模式
void ThreadPool::setMode(PoolMode mode) {
	if (checkRunningState())return;
	poolMode_ = mode;
}
//设置线程数量上限阈值cached模式下
void ThreadPool::setthreadSizeThreshHold(int threadhold) {
	if (checkRunningState())return;
	if(poolMode_==PoolMode::MODE_CACHED)//只有cached模式才可以设置
	threadSizeThreshHold_ = threadhold;
}

//设置task任务队列上线阈值
void ThreadPool::settaskQueMaxThreshHold(int threshhold) {
	if (checkRunningState())return;
	taskQueMaxThreshHold_ = threshhold;
}

//给线程池提交任务
Result ThreadPool::submitTask(shared_ptr<Task>sp) {
	 //获取锁
	unique_lock<mutex>lock(taskQueMtx_);
	//线程通信等待
	/*while (taseQue_.size() == taskQueMaxThreshHold_) {
		notFull_.wait(lock);
	}*/
	if (!notFull_.wait_for(lock, chrono::seconds(1),
		[&]()->bool {return taseQue_.size() < taskQueMaxThreshHold_; })) {
		cerr << "超时了任务提交失败" << endl;
		lock.unlock();
		return Result(sp,false);
	}
	cout << "提交成功\n";
	taseQue_.emplace(sp);
	tasksize_++;

	notEmpty_.notify_all(); 
	//cached模式适合小而快的任务，根据任务数量动态调整线程个数
	if (poolMode_ == PoolMode::MODE_CACHED
		&& tasksize_ > idleThreadSize_
		&& curThreadSize_ < threadSizeThreshHold_) {
		//创建新线程
		/*auto ptr = make_unique<Thread>(bind(&ThreadPool::threadFunc, this));
		threads_.emplace_back(move(ptr));*/
		auto ptr = make_unique<Thread>(bind(&ThreadPool::threadFunc, this, placeholders::_1));
		int threadid = ptr->getId();
		threads_.emplace(threadid, move(ptr));
		threads_[threadid]->start();//启动线程
		
		//修改线程相关变量
		curThreadSize_++;
		idleThreadSize_++;
		cout << "creat new thread\n";
	}
	return Result(sp);
}

//开启线程池
void ThreadPool::start(int initThreadSize ) {
	//设置线程池的运行状态
	isPoolRunning = true;

	initThreadSize_ = initThreadSize;
	curThreadSize_= initThreadSize;
	///创建线程对象
	for (int i = 0; i < initThreadSize_; i++) {
		auto ptr = make_unique<Thread>( bind(&ThreadPool::threadFunc, this,placeholders::_1));
		int threadid=ptr->getId();
		threads_.emplace(threadid,move(ptr));
		//threads_.emplace_back(move(ptr));
	}

	///启动所有线程
	for (int i = 0; i < initThreadSize_; i++) {
		(threads_[i])->start();
		idleThreadSize_++;//记录初始空闲线程
	}
}
//定义线程函数
void ThreadPool::threadFunc(int threadid) {
	/*cout << "begin tid:" << this_thread::get_id() << endl;
	cout << "end tid:" << this_thread::get_id() << endl;*/
	auto lastTime = std::chrono::high_resolution_clock().now();
	for (;  ;) {
		shared_ptr<Task>task;
		{
			//获取锁
			unique_lock<mutex>lock(taskQueMtx_);
			//cached模式下，有可能已经创建了很多线程，但是空闲时间超过60s，需要回收
			// 超过阈值的部分
			// 当前时间-上一次线程执行时间>60s
			while ( taseQue_.size() == 0) { 
				if (!isPoolRunning) {
					threads_.erase(threadid);
					cout << "threadid" << this_thread::get_id() << "exit\n";
					exitcond_.notify_all();
					return;
				}
				if (poolMode_ == PoolMode::MODE_CACHED) {
					if (cv_status::timeout
						== notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						auto now = chrono::high_resolution_clock::now();
						auto dur = chrono::duration_cast<chrono::seconds>(now - lastTime);
						if (dur.count() >= THREAD_MAX_IDLE_TIME
							&& curThreadSize_ > initThreadSize_) {
							//开始回收进程
							threads_.erase(threadid);
							curThreadSize_--;
							idleThreadSize_--;
							cout << "threadid" << this_thread::get_id() << "exit\n";

						}
					}
					if (!isPoolRunning) {
						threads_.erase(threadid);
						cout << "threadid" << this_thread::get_id() << "exit\n";
						exitcond_.notify_all();
						return;
					}
				}
				else {
					//等待notempty条件
					notEmpty_.wait(lock, [&]() {return taseQue_.size() > 0; });
				}
			}
			 
			idleThreadSize_--;
			//任务队列取出一个函数
			task = taseQue_.front();
			taseQue_.pop();
			tasksize_--;
			cout << "id" << this_thread::get_id() << "任务开始！" << endl;
			if (taseQue_.size() > 0)notFull_.notify_all();
			//释放锁
			notFull_.notify_all();
			lock.unlock();
		}
		 
		//当前线程执行这个项目
			if (task != nullptr)
				task->exec();
		cout << "id" << this_thread::get_id() << "任务结束了~\n" << endl;
		idleThreadSize_++;
		//更新任务执行的时间
		lastTime = std::chrono::high_resolution_clock().now();
	}
	
}
bool ThreadPool::checkRunningState()const {
	return isPoolRunning;
}

/*=========================================================*/
/*线程方法实现*/
//线程构造
int Thread::gengeratid_ = 0;
Thread::Thread(ThreadFunc func) 
	:func_(func),
	threadId_(gengeratid_++)
{
}
//线程析构
Thread::~Thread() {

}
//启动线程
void Thread::start() {
	//创建一个线程执行线程函数
	thread t(func_,threadId_);
	t.detach();//设计成分离线程
}
int Thread::getId()const {
	return threadId_;
}
///  
/// Task实现
/// 
void Task::exec() {
	result_->setVal(run());
}
void Task::setResult(Result* res) {
	result_ = res;
}





///////////////////////////////
//Result方法实现
Result::Result(shared_ptr<Task>task, bool isValid) :isValid_(isValid),
task_(task)
{
	task_->setResult(this);
}
Any Result::get() {
	if (!isValid_)return "";
	sem_.wait();//如果没有执行完毕，会阻塞用户线程
	return move(any_);
}

void Result::setVal(Any any) {
	this->any_ = move(any);
	sem_.post();
}