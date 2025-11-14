#include "httplib.h"

int main() 
{
    httplib::Server svr;

    svr.Post("/analyze", [](const httplib::Request& req, httplib::Response& res) 
    {
        std::string result = "{ \"tempo\": 120 }";
        res.set_content(result, "application/json");
    });

    svr.listen("127.0.0.1", 5000);
    return 0;
}