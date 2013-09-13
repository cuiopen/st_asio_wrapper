/*
 * st_asio_wrapper_service_pump.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at both server and client endpoint
 */

#ifndef ST_ASIO_WRAPPER_SERVICE_PUMP_H_
#define ST_ASIO_WRAPPER_SERVICE_PUMP_H_

#include <boost/container/list.hpp>

#include "st_asio_wrapper_base.h"

//io thread number
//listen, all msg send and recv, msg handle(on_msg_handle() and on_msg()) will use these threads
//keep big enough, empirical value need you to try to find out in your own environment
#ifndef ST_SERVICE_THREAD_NUM
#define ST_SERVICE_THREAD_NUM 8
#endif

namespace st_asio_wrapper
{

class st_service_pump : public io_service
{
public:
	class i_service
	{
	protected:
		i_service(st_service_pump& service_pump_) : service_pump(service_pump_), id(0) {service_pump_.add(this);}
		virtual ~i_service() {}

	public:
		virtual void init() = 0;
		virtual void uninit() = 0;

		void set_id(int _id) {id = _id;}
		int get_id() const {return id;}
		bool is_equal_to(int id) const {return get_id() == id;}

		st_service_pump& get_service_pump() {return service_pump;}
		const st_service_pump& get_service_pump() const {return service_pump;}

	protected:
		st_service_pump& service_pump;

	private:
		int id;
	};

public:
	st_service_pump() : started(false) {}
	virtual ~st_service_pump() {clear();}

	i_service* find(int id)
	{
		BOOST_AUTO(iter, std::find_if(service_can.begin(), service_can.end(),
			std::bind2nd(std::mem_fun(&i_service::is_equal_to), id)));
		return iter == service_can.end() ? NULL : *iter;
	}
	void remove(i_service* i_service_) {assert(NULL != i_service_); free(i_service_); service_can.remove(i_service_);}
	void remove(int id)
	{
		BOOST_AUTO(iter, std::find_if(service_can.begin(), service_can.end(),
			std::bind2nd(std::mem_fun(&i_service::is_equal_to), id)));
		if (iter != service_can.end())
		{
			free(*iter);
			service_can.erase(iter);
		}
	}
	void clear() {do_something_to_all(boost::bind(&st_service_pump::free, this, _1)); service_can.clear();}

	void start_service(int thread_num = ST_SERVICE_THREAD_NUM)
	{
		if (!is_service_started())
		{
			service_thread = thread(boost::bind(&st_service_pump::run_service, this, thread_num));
			while (!is_service_started())
				this_thread::sleep(get_system_time() + posix_time::milliseconds(50));
		}
	}
	//stop the service, must be invoked explicitly when the service need to stop, for example,
	//close the application
	void stop_service()
	{
		if (is_service_started())
		{
			do_something_to_all(boost::mem_fn(&i_service::uninit));
			service_thread.join();
		}
	}

	//only used when stop_service() can not stop the service(been blocked and can not return)
	void force_stop_service(){stop(); service_thread.join();}
	bool is_running() const {return !stopped();}
	bool is_service_started() const {return started;}

	//this function works like start_service except that it will block until service run out,
	//do not invoke stop_service from other thread(because this thread has been blocked) to unblock it,
	//instead, use end_service(also from other thread)
	void run_service(int thread_num = ST_SERVICE_THREAD_NUM)
	{
		reset(); //this is needed when re-start_service
		do_something_to_all(boost::mem_fn(&i_service::init));
		do_service(thread_num);
	}
	//stop the service, must be invoked explicitly when the service need to stop, for example,
	//close the application
	void end_service()
	{
		do_something_to_all(boost::mem_fn(&i_service::uninit));
		while (is_service_started())
			this_thread::sleep(get_system_time() + posix_time::milliseconds(50));
	}

protected:
	virtual void free(i_service* i_service_) {} //if needed, rewrite this to free the service

#ifdef ENHANCED_STABILITY
	virtual bool on_exception(const std::exception& e)
	{
		unified_out::info_out("service pump exception: %s.", e.what());
		return true; //continue this service pump
	}

	size_t run(error_code& ec)
	{
		while (true)
		{
			try {return io_service::run(ec);}
			catch (const std::exception& e) {if (!on_exception(e)) return 0;}
		}
	}
#endif

	DO_SOMETHING_TO_ALL(service_can)
	DO_SOMETHING_TO_ONE(service_can)

private:
	void add(i_service* i_service_) {assert(NULL != i_service_); service_can.push_back(i_service_);}

	void do_service(int thread_num)
	{
		started = true;
		unified_out::info_out("service pump started.");

		--thread_num;
		thread_group tg;
		for (int i = 0; i < thread_num; ++i)
			tg.create_thread(boost::bind(&st_service_pump::run, this, error_code()));
		error_code ec;
		run(ec);

		if (thread_num > 0)
			tg.join_all();

		unified_out::info_out("service pump end.");
		started = false;
	}

protected:
	container::list<i_service*> service_can; //not protected by mutex, please note
	thread service_thread;
	bool started;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_SERVICE_PUMP_H_ */
