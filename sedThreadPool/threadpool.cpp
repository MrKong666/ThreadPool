#include"threadpool.h"

const int TASK_MAX_THRESHHOLE = 1024;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 10;//��λ��
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
	
	unique_lock<mutex>lock(taskQueMtx_);//�����ڻ��ѻ�����������
	notEmpty_.notify_all();//�������̻߳�������
	//�ȴ������߳��˳�
	exitcond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}

//����̳߳ع���ģʽ
void ThreadPool::setMode(PoolMode mode) {
	if (checkRunningState())return;
	poolMode_ = mode;
}
//�����߳�����������ֵcachedģʽ��
void ThreadPool::setthreadSizeThreshHold(int threadhold) {
	if (checkRunningState())return;
	if(poolMode_==PoolMode::MODE_CACHED)//ֻ��cachedģʽ�ſ�������
	threadSizeThreshHold_ = threadhold;
}

//����task�������������ֵ
void ThreadPool::settaskQueMaxThreshHold(int threshhold) {
	if (checkRunningState())return;
	taskQueMaxThreshHold_ = threshhold;
}

//���̳߳��ύ����
Result ThreadPool::submitTask(shared_ptr<Task>sp) {
	 //��ȡ��
	unique_lock<mutex>lock(taskQueMtx_);
	//�߳�ͨ�ŵȴ�
	/*while (taseQue_.size() == taskQueMaxThreshHold_) {
		notFull_.wait(lock);
	}*/
	if (!notFull_.wait_for(lock, chrono::seconds(1),
		[&]()->bool {return taseQue_.size() < taskQueMaxThreshHold_; })) {
		cerr << "��ʱ�������ύʧ��" << endl;
		lock.unlock();
		return Result(sp,false);
	}
	cout << "�ύ�ɹ�\n";
	taseQue_.emplace(sp);
	tasksize_++;

	notEmpty_.notify_all(); 
	//cachedģʽ�ʺ�С��������񣬸�������������̬�����̸߳���
	if (poolMode_ == PoolMode::MODE_CACHED
		&& tasksize_ > idleThreadSize_
		&& curThreadSize_ < threadSizeThreshHold_) {
		//�������߳�
		/*auto ptr = make_unique<Thread>(bind(&ThreadPool::threadFunc, this));
		threads_.emplace_back(move(ptr));*/
		auto ptr = make_unique<Thread>(bind(&ThreadPool::threadFunc, this, placeholders::_1));
		int threadid = ptr->getId();
		threads_.emplace(threadid, move(ptr));
		threads_[threadid]->start();//�����߳�
		
		//�޸��߳���ر���
		curThreadSize_++;
		idleThreadSize_++;
		cout << "creat new thread\n";
	}
	return Result(sp);
}

//�����̳߳�
void ThreadPool::start(int initThreadSize ) {
	//�����̳߳ص�����״̬
	isPoolRunning = true;

	initThreadSize_ = initThreadSize;
	curThreadSize_= initThreadSize;
	///�����̶߳���
	for (int i = 0; i < initThreadSize_; i++) {
		auto ptr = make_unique<Thread>( bind(&ThreadPool::threadFunc, this,placeholders::_1));
		int threadid=ptr->getId();
		threads_.emplace(threadid,move(ptr));
		//threads_.emplace_back(move(ptr));
	}

	///���������߳�
	for (int i = 0; i < initThreadSize_; i++) {
		(threads_[i])->start();
		idleThreadSize_++;//��¼��ʼ�����߳�
	}
}
//�����̺߳���
void ThreadPool::threadFunc(int threadid) {
	/*cout << "begin tid:" << this_thread::get_id() << endl;
	cout << "end tid:" << this_thread::get_id() << endl;*/
	auto lastTime = std::chrono::high_resolution_clock().now();
	for (;  ;) {
		shared_ptr<Task>task;
		{
			//��ȡ��
			unique_lock<mutex>lock(taskQueMtx_);
			//cachedģʽ�£��п����Ѿ������˺ܶ��̣߳����ǿ���ʱ�䳬��60s����Ҫ����
			// ������ֵ�Ĳ���
			// ��ǰʱ��-��һ���߳�ִ��ʱ��>60s
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
							//��ʼ���ս���
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
					//�ȴ�notempty����
					notEmpty_.wait(lock, [&]() {return taseQue_.size() > 0; });
				}
			}
			 
			idleThreadSize_--;
			//�������ȡ��һ������
			task = taseQue_.front();
			taseQue_.pop();
			tasksize_--;
			cout << "id" << this_thread::get_id() << "����ʼ��" << endl;
			if (taseQue_.size() > 0)notFull_.notify_all();
			//�ͷ���
			notFull_.notify_all();
			lock.unlock();
		}
		 
		//��ǰ�߳�ִ�������Ŀ
			if (task != nullptr)
				task->exec();
		cout << "id" << this_thread::get_id() << "���������~\n" << endl;
		idleThreadSize_++;
		//��������ִ�е�ʱ��
		lastTime = std::chrono::high_resolution_clock().now();
	}
	
}
bool ThreadPool::checkRunningState()const {
	return isPoolRunning;
}

/*=========================================================*/
/*�̷߳���ʵ��*/
//�̹߳���
int Thread::gengeratid_ = 0;
Thread::Thread(ThreadFunc func) 
	:func_(func),
	threadId_(gengeratid_++)
{
}
//�߳�����
Thread::~Thread() {

}
//�����߳�
void Thread::start() {
	//����һ���߳�ִ���̺߳���
	thread t(func_,threadId_);
	t.detach();//��Ƴɷ����߳�
}
int Thread::getId()const {
	return threadId_;
}
///  
/// Taskʵ��
/// 
void Task::exec() {
	result_->setVal(run());
}
void Task::setResult(Result* res) {
	result_ = res;
}





///////////////////////////////
//Result����ʵ��
Result::Result(shared_ptr<Task>task, bool isValid) :isValid_(isValid),
task_(task)
{
	task_->setResult(this);
}
Any Result::get() {
	if (!isValid_)return "";
	sem_.wait();//���û��ִ����ϣ��������û��߳�
	return move(any_);
}

void Result::setVal(Any any) {
	this->any_ = move(any);
	sem_.post();
}