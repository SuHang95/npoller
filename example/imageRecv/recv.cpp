#include"recv.h"

namespace Recv{

	logger debug("debugLog",logger::DEBUG);

	recv::recv(unsigned short int __port,logger _log,
		uint8_t _use):log(_log),proc(_log),use(_use){

		size=0;
		
		fd=socket(AF_INET,SOCK_STREAM,0);
		sockaddr_in address;
		socklen_t len;

		if(fd<0){
			log.error("Socket file descriptor get fail:");
			exit(1);
		}

		address.sin_family=AF_INET;
		address.sin_addr.s_addr=htonl(INADDR_ANY);
		address.sin_port=htons(__port);                                            
		len=sizeof(address);

		try{
	
			int ret=bind(fd,(struct sockaddr*)&address,len);
			if(ret<0){
				log.error("Bind fail:");
				exit(1);
			}

			port=__port;

			std::thread _accept([this](){
				this->accept();
			});
			_accept.detach();

			std::thread _process([this](){
				(this->proc).process();
			});
			_process.detach();
		
		} catch(std::exception e){
			std::cout<<"An exception happened !,what is"<<e.what()<<std::endl;
			exit(1);
		}
	}	

	recv::~recv(){
		log.info("A recv instance will be destroyed!");
	}	
	void recv::accept(){
		int _fd;
		struct sockaddr_in _addr;
		socklen_t _len;

		int ret=listen(fd,0);
		if(ret<0){
			log.error("Listen fail");
			exit(1);
		}
	
		while(1){
			_fd=::accept(fd,(sockaddr*)&_addr,&_len);
			debug.info("A connection was return!");
			if(_fd<0){
				log.error("Accept fail");
				continue;		
			}
			else{
				std::shared_ptr<tcp> client(new tcp(_fd,2,log));
				if(proc.add_io(client)){
					std::shared_ptr<recvtask> newtask(new recvtask(client,log,this));	
					task_mutex.lock();
					task_list.push_back(newtask);
					task_mutex.unlock();
				}
			}
			task_mutex.lock();
			for(auto it=task_list.begin();it!=task_list.end();it++){
				if((*it)->state==0){
					auto it1=it;
					it--;
					task_list.erase(it1);
					log.debug("Erase a dead task!");
				}
			}
			task_mutex.unlock();
		}
	}

	void recv::addframe(const frame& _frame){
		std::unique_lock<std::mutex> lk(frame_mutex);

		auto f=[this](auto& result,const frame& _frame){
			result.push(_frame);
			size=result.size();
		};

		if(use==0){
			f(result_small,_frame);
		}else{
			f(result_big,_frame);
		}

		preque_cond.notify_all();
	}

	frame recv::getframe(){
		std::unique_lock<std::mutex> lk(frame_mutex);

		auto f=[this](auto& result)->frame{
			if(size!=result.size()){
				log.warn("The recv.size %d not equals to result.size() %d"
					,size.load(),result_small.size());
				size=result.size();
			}
			if(size>0){
				frame ret=result.top();
				result.pop();
				size--;
				return ret;
			}else{
				log.warn("There are empty in result list");
			}
			return frame();
		};

		preque_cond.wait(lk,[this]()->bool{return (this->size>0);});

		if(use==0){
			return f(result_small);
		}else{
			return f(result_big);
		}
	}

}
