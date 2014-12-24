#include "shared_collection.hpp"

namespace netcom_impl {
    shared_collection_base::shared_collection_base(shared_collection_factory& factory, netcom_base& net,
        const std::string& name, shared_collection_id_t id) :
        factory_(factory), net_(net), id(id), name(name) {}

    shared_collection_base::~shared_collection_base() {
        disconnect();
    }

    void shared_collection_base::destroy() {
        factory_.destroy_(id);
    }

    void shared_collection_base::connect() {
        disconnect();
        check_valid_();
        connected_ = true;
    }

    void shared_collection_base::disconnect() {
        clients_.clear();
        connected_ = false;
    }

    bool shared_collection_base::is_connected() const {
        return connected_;
    }

    void shared_collection_base::register_client(observe_request&& req) {
        if (connected_) {
            // TODO: remove the rvalue ref, they are misleading here
            register_and_send_collection_(std::move(req));
            if (!req.failed()) {
                clients_.insert(req.packet.from);
            }
        } else {
            req.unhandle();
        }
    }

    void shared_collection_base::unregister_client(actor_id_t cid) {
        clients_.erase(cid);
    }
}

void shared_collection_factory::destroy_(shared_collection_id_t id) {
    collections_.erase(id);
    id_provider_.free_id(id);
}

shared_collection_factory::shared_collection_factory(netcom_base& net) : net_(net) {
    pool_ << net_.watch_request(
        [this](netcom_base::request_t<request::get_shared_collection_id>&& req) {
            for (auto& c : collections_) {
                if (c->name == req.arg.name) {
                    req.answer(c->id);
                    return;
                }
            }

            req.fail(request::get_shared_collection_id::failure::reason::no_such_collection);
        }
    );

    pool_ << net_.watch_request(
        [this](netcom_base::request_t<request::observe_shared_collection>&& req) {
            auto iter = collections_.find(req.arg.id);
            if (iter != collections_.end()) {
                (*iter)->register_client(std::move(req));
            } else {
                req.unhandle();
            }
        }
    );

    pool_ << net_.watch_message(
        [this](const netcom_base::message_t<message::leave_shared_collection>& msg) {
            auto iter = collections_.find(msg.arg.id);
            if (iter != collections_.end()) {
                (*iter)->unregister_client(msg.packet.from);
            }
        }
    );

    pool_ << net_.watch_message(
        [this](const message::client_disconnected& msg) {
            for (auto& c : collections_) {
                c->unregister_client(msg.id);
            }
        }
    );

    pool_ << net_.watch_message(
        [this](const netcom_base::message_t<message::shared_collection_add>& msg) {
            auto iter = observers_.find(msg.arg.id);
            if (iter == observers_.end()) return;
            (*iter)->add_item(msg);
        }
    );

    pool_ << net_.watch_message(
        [this](const netcom_base::message_t<message::shared_collection_remove>& msg) {
            auto iter = observers_.find(msg.arg.id);
            if (iter == observers_.end()) return;
            (*iter)->remove_item(msg);
        }
    );

    pool_ << net_.watch_message(
        [this](const netcom_base::message_t<message::shared_collection_clear>& msg) {
            auto iter = observers_.find(msg.arg.id);
            if (iter == observers_.end()) return;
            (*iter)->clear(msg);
        }
    );
}

void shared_collection_factory::clear() {
    observers_.clear();

    while (!collections_.empty()) {
        (*collections_.begin())->destroy();
    }
}
