//
// blocking_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2024 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <iostream>
#include <thread>
#include <utility>
#include "asio.hpp"
#include <ctime>
#include <queue>
#include "Request.h"
#include <map>
#include <set>

using asio::ip::tcp;

const int max_length = 1024;

class User {
public:
    User(std::string name = "User") {
        count++;
        id_ = count;
        this->name_ = name + std::to_string(id_);
        rub_ = 0, usd_ = 0;
    }

    User(int id) {
        count++;
        this->id_ = id;
        this->name_ = "User" + std::to_string(id);
        rub_ = 0, usd_ = 0;
    }

    int& id() { return id_; }
    const int& id() const{ return id_; }

    std::string& name() { return name_; }
    const std::string& name() const { return name_; }

    int& rub() { return rub_; }
    const int& rub() const { return rub_; }

    int& usd() { return usd_; }
    const int& usd() const { return usd_; }

    void change_value(int rub, int usd) {
        rub_ += rub;
        usd_ += usd;
    }

    static int count;
private: 
    std::string name_;
    int id_; 
    int rub_, usd_;
};

std::ostream& operator<<(std::ostream& os, const User& usr)
{
    os << "ID: " << usr.id() << " Name:  " << usr.name() << "\nRub: " << usr.rub() << " Usd : " << usr.usd() << "\n";
    return os;
}

int User::count = 0;

class BD {
public:
    //BD();
    User& operator[](int idx);
    const int max_users = 10000;

    User& add_user(User usr) {
        std::cout << "adding user: \n" << usr;
        if (ids.find(usr.id()) == ids.end()) {
            ids.insert(usr.id());
            users[usr.id()] = usr;
        }
        return users[usr.id()];
    }

    User& add_user(int id) {
        std::cout << "adding user\n";
        if (ids.find(id) == ids.end()) {
            ids.insert(id);
            users[id] = User(id);
        }
        return users[id];
    }

    void change_user(int id, int rub, int usd) {
        users[id].change_value(rub, usd);
    }

    friend std::ostream& operator<<(std::ostream& os, const BD& bd);

private:
    std::map<int, User>users;
    std::set<int>ids;
};

User& BD::operator[](int idx) {
    return users[idx];
}

std::ostream& operator<<(std::ostream& os, const BD& bd)
{
    for (const auto& elem : bd.users)
        std::cout << "-----------\n" << elem.first << "\n" << elem.second << "-----------\n";
    return os;
}
// std::vector<Request> bd;

//struct comp {
//    bool operator()(const Request& a, const Request& b)
//    {
//        if (a.get_rub() != b.get_rub())return a.get_rub() < b.get_rub();
//        return a.get_time() < b.get_time();
//    }
//};


BD bd;
std::priority_queue<Request>requests; // Приоритетная очередь с запросами

void session(tcp::socket sock)
{
    try
    {
        for (;;)
        {
            int data[4]; // Массив, куда мы принимаем данные от клиента
            std::error_code error;
            size_t length = sock.read_some(asio::buffer(data), error);

            if (error == asio::error::eof)
                break; // Connection closed cleanly by peer.
            else if (error)
                throw std::system_error(error); // Some other error.

            //std::cout << "data: " << data[0] << " " << data[1] << " " << data[2] <<  " " << data[3] << "\n";

            User curr_usr = bd.add_user(data[3]);

            //std::cout << curr_usr;

            Request req(data); // Надо сделать конструктор присвоения чтобы не создавать объект каждую итерацию

            int answer[2] = { 200, 0 }; // Проверка запроса и запись кода ответа
            if (std::abs(data[0]) > 1)answer[0] = 100;
            else if (data[1] <= 0 || data[2] <= 0)answer[0] = 100;
            
            if (requests.empty())requests.push(req);
            else {
                while (!requests.empty() && req.get_usd() > 0 && requests.top().get_option() != req.get_option()) {
                    auto top_req = requests.top(); // top_req - ссылка на последнюю не закрытую сделку.
                    //std::cout << "DEBUG: " << top_req.get_id() << " " << req.get_id() << "\n";
                    int top_id = top_req.get_id(), req_id = req.get_id();
                    User top_user = bd[top_id];
                    User req_user = bd[req_id];
                    int best = std::max(req.get_rub() * top_req.get_option(), top_req.get_rub() * top_req.get_option()) * top_req.get_option();
                    std::cout << "BEST: " << best << "\n";
                    if (top_req.get_usd() > req.get_usd()) {
                        //std::cout << "Сделка состоялась " << req << requests.top();
                        int top_usd = top_req.get_usd();
                        requests.pop();

                        
                        top_user.change_value(-top_req.get_option() * req.get_usd() * top_req.get_rub(), top_req.get_option() * req.get_usd());
                        req_user.change_value(-req.get_option() * req.get_usd() * best, req.get_option() * req.get_usd());

                        bd[top_id] = top_user;
                        bd[req_id] = req_user;

                        top_req.change_usd(top_req.get_option() * req.get_usd());
                        std::cout << "TOP Change: " << top_req.get_option() * req.get_usd() << "\n";
                        req.set_usd(0);

                        requests.push(top_req);
                    }
                    else {
                        //int best = std::min(req.get_rub() * top_req.get_option(), top_req.get_rub() * top_req.get_option()) * top_req.get_option();
                        //std::cout << "Сделка состоялась " << req << requests.top();
                        top_user.change_value(-top_req.get_option() * top_req.get_usd() * top_req.get_rub(), top_req.get_option() * top_req.get_usd());
                        req_user.change_value(-req.get_option() * top_req.get_usd() * best, req.get_option() * top_req.get_usd());
                        bd[top_id] = top_user;
                        bd[req_id] = req_user;
                        req.change_usd(req.get_option() * top_req.get_usd());
                        std::cout << "REQ Change: " << top_req.get_option() * req.get_usd() << "\n";
                        requests.pop();
                    }
                    std::cout << top_user << " " << req_user << "-\n";
                    //top_user.rub() = 999;
                    //std::cout << req << "|" << requests.top();
                }

                if (req.get_usd() != 0)requests.push(req);
            }

            std::cout << bd;
            std::cout << "Последняя заявка: " << requests.top();
            asio::write(sock, asio::buffer(answer, sizeof answer));
            //bd.push_back(Request(data));
            
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception in thread: " << e.what() << "\n";
    }
}

void server(asio::io_context& io_context, unsigned short port)
{
    tcp::acceptor a(io_context, tcp::endpoint(tcp::v4(), port));
    for (;;)
    {
        std::thread(session, a.accept()).detach();
    }
}

int main(int argc, char* argv[])
{
    std::cout << "Server\n";
    std::setlocale(LC_ALL, "RU");
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: blocking_tcp_echo_server <port>\n";
            return 1;
        }

        asio::io_context io_context;

        server(io_context, std::atoi(argv[1]));
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}