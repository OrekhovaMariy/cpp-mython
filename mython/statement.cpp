#include "statement.h"

#include <iostream>
#include <sstream>
#include <utility>

using namespace std;

namespace ast {

    using runtime::Closure;
    using runtime::Context;
    using runtime::ObjectHolder;

    namespace {
        const string ADD_METHOD = "__add__"s;
        const string INIT_METHOD = "__init__"s;
    }  // namespace

    ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
        closure[var_] = rv_->Execute(closure, context);
        return closure[var_];
    }

    Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv) 
        : var_(std::move(var))
        , rv_(std::move(rv)) {}

    VariableValue::VariableValue(const std::string& var_name) {
        dotted_ids_.push_back(var_name);
    }

    VariableValue::VariableValue(std::vector<std::string> dotted_ids)
        : dotted_ids_(std::move(dotted_ids)) {}
    

    ObjectHolder VariableValue::Execute(Closure& closure, [[maybe_unused]] Context& context) {
        Closure* closure_ptr = &closure;
        for (size_t i = 0; i < dotted_ids_.size(); ++i) {
            const std::string& field_name = dotted_ids_[i];
            if (closure_ptr->count(field_name) == 0) {
                throw std::runtime_error("Cant find var"s);
            }
            if (i == dotted_ids_.size() - 1) {
                return closure_ptr->at(field_name);
            }
            auto ptr_obj = closure_ptr->at(field_name).TryAs<runtime::ClassInstance>();
            if (!ptr_obj) {
                throw std::runtime_error("This isn't object"s);
            }
            closure_ptr = &ptr_obj->Fields();
        }
        return {};
    }

    unique_ptr<Print> Print::Variable(const std::string& name) {
        return std::make_unique<Print>(std::make_unique<VariableValue>(name));
    }

    Print::Print(unique_ptr<Statement> argument) {
        args_.push_back(std::move(argument));
    }

    Print::Print(vector<unique_ptr<Statement>> args) 
        : args_(std::move(args)){}

    ObjectHolder Print::Execute(Closure& closure, Context& context) {
        ObjectHolder obj;
        bool first_arg = true;
        for (const auto& arg : args_) {
            if (!first_arg) {
                context.GetOutputStream() << " "s;
            }
            first_arg = false;
            obj = arg->Execute(closure, context);
            if (obj) {
                obj->Print(context.GetOutputStream(), context);
            }
            else {
                context.GetOutputStream() << "None"s;
            }
        }
        context.GetOutputStream() << "\n"s;
        return {};
    }

    MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
        std::vector<std::unique_ptr<Statement>> args) 
        : object_(std::move(object))
        , method_(std::move(method))
        , args_(std::move(args)) {}

    ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
        const auto obj = object_->Execute(closure, context);
        const auto class_instance_ptr = obj.TryAs<runtime::ClassInstance>();
        if (obj && class_instance_ptr && class_instance_ptr->HasMethod(method_, args_.size())) {
            std::vector<runtime::ObjectHolder> actual_args;
            for (const auto& arg : args_) {
                actual_args.push_back(arg->Execute(closure, context));
            }
            return class_instance_ptr->Call(method_, actual_args, context);
        }
        return {};
    }

    ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
        auto obj = argument_->Execute(closure, context);
        if (!obj) {
            return ObjectHolder::Own(runtime::String{ "None"s });
        }
        runtime::DummyContext dummy_context;
        obj->Print(dummy_context.output, dummy_context);
        return ObjectHolder::Own(runtime::String{ dummy_context.output.str() });
    }

    ObjectHolder Add::Execute(Closure& closure, Context& context) {
        auto lhs = lhs_->Execute(closure, context);
        auto rhs = rhs_->Execute(closure, context);

        auto lhs_number = lhs.TryAs<runtime::Number>();
        auto rhs_number = rhs.TryAs<runtime::Number>();
        if (lhs_number != nullptr && rhs_number != nullptr) {
            int number = lhs_number->GetValue() + rhs_number->GetValue();
            return runtime::ObjectHolder::Own(runtime::Number{ number });
        }

        auto lhs_string = lhs.TryAs<runtime::String>();
        auto rhs_string = rhs.TryAs<runtime::String>();
        if (lhs_string != nullptr && rhs_string != nullptr) {
            string str = lhs_string->GetValue() + rhs_string->GetValue();
            return runtime::ObjectHolder::Own(runtime::String{ std::move(str) });
        }

        auto lhs_instance = lhs.TryAs<runtime::ClassInstance>();
        if (lhs_instance != nullptr && lhs_instance->HasMethod(ADD_METHOD, 1)) {
            std::vector<ObjectHolder> actual_args = { rhs };
            return lhs_instance->Call(ADD_METHOD, actual_args, context);
        }

        throw std::runtime_error("No __add__ method"s);
    }

    ObjectHolder Sub::Execute(Closure& closure, Context& context) {
        auto lhs = lhs_->Execute(closure, context);
        auto rhs = rhs_->Execute(closure, context);

        auto lhs_number = lhs.TryAs<runtime::Number>();
        auto rhs_number = rhs.TryAs<runtime::Number>();
        if (lhs_number != nullptr && rhs_number != nullptr) {
            int number = lhs_number->GetValue() - rhs_number->GetValue();
            return runtime::ObjectHolder::Own(runtime::Number{ number });
        }

        throw std::runtime_error("lhs or rhs not Number"s);
    }

    ObjectHolder Mult::Execute(Closure& closure, Context& context) {
        auto lhs = lhs_->Execute(closure, context);
        auto rhs = rhs_->Execute(closure, context);

        auto lhs_number = lhs.TryAs<runtime::Number>();
        auto rhs_number = rhs.TryAs<runtime::Number>();
        if (lhs_number != nullptr && rhs_number != nullptr) {
            int number = lhs_number->GetValue() * rhs_number->GetValue();
            return runtime::ObjectHolder::Own(runtime::Number{ number });
        }

        throw std::runtime_error("lhs or rhs not Number"s);
    }

    ObjectHolder Div::Execute(Closure& closure, Context& context) {
        auto lhs = lhs_->Execute(closure, context);
        auto rhs = rhs_->Execute(closure, context);

        auto lhs_number = lhs.TryAs<runtime::Number>();
        auto rhs_number = rhs.TryAs<runtime::Number>();
        if (lhs_number != nullptr && rhs_number != nullptr) {
            if (rhs_number->GetValue() == 0) {
                throw std::runtime_error("Division by zero"s);
            }

            int number = lhs_number->GetValue() / rhs_number->GetValue();
            return runtime::ObjectHolder::Own(runtime::Number{ number });
        }

        throw std::runtime_error("lhs or rhs not Number"s);
    }

    void Compound::AddStatement(std::unique_ptr<Statement> stmt)
    {
        stmt_.push_back(std::move(stmt));
    }

    ObjectHolder Compound::Execute(Closure& closure, [[maybe_unused]] Context& context) {
        for (const auto& statement : stmt_) {
            statement->Execute(closure, context);
        }
        return {};
    }

    
    Return::Return(std::unique_ptr<Statement> statement)
        : statement_(std::move(statement)) {}

    ObjectHolder Return::Execute(Closure& closure, Context& context) {
        throw statement_->Execute(closure, context);
    }

    ClassDefinition::ClassDefinition(ObjectHolder cls) 
        : cls_(cls){}

    ObjectHolder ClassDefinition::Execute(Closure& closure, [[maybe_unused]] Context& context) {
        const auto obj = cls_.TryAs<runtime::Class>();
        closure[obj->GetName()] = std::move(cls_);
        return {};
    }

    FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
        std::unique_ptr<Statement> rv)
        : object_(std::move(object))
        , field_name_(std::move(field_name))
        , rv_(std::move(rv)) {}

    ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
        const auto obj = object_.Execute(closure, context);
        const auto class_inst_ptr = obj.TryAs<runtime::ClassInstance>();
        if (class_inst_ptr) {
            class_inst_ptr->Fields()[field_name_] = rv_->Execute(closure, context);
            return class_inst_ptr->Fields()[field_name_];
        }
        throw std::runtime_error("Cant find field"s);
    }

    IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
        std::unique_ptr<Statement> else_body) 
        : condition_(std::move(condition))
        , if_body_(std::move(if_body))
        , else_body_(std::move(else_body)){}

    ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
        if (runtime::IsTrue(condition_->Execute(closure, context))) {
            return if_body_->Execute(closure, context);
        }
        else if (else_body_ != nullptr) {
            return else_body_->Execute(closure, context);
        }
        else {
            return ObjectHolder::None();
        }
    }

    ObjectHolder Or::Execute(Closure& closure, Context& context) {
        auto lhs = lhs_->Execute(closure, context);
        auto rhs = rhs_->Execute(closure, context);
        bool result = (runtime::IsTrue(lhs) || runtime::IsTrue(rhs));
        return runtime::ObjectHolder::Own(runtime::Bool{ result });
    }

    ObjectHolder And::Execute(Closure& closure, Context& context) {
        auto lhs = lhs_->Execute(closure, context);
        auto rhs = rhs_->Execute(closure, context);
        bool result = (runtime::IsTrue(lhs) && runtime::IsTrue(rhs));
        return runtime::ObjectHolder::Own(runtime::Bool{ result });
    }

    ObjectHolder Not::Execute(Closure& closure, Context& context) {
        auto arg = argument_->Execute(closure, context);
        bool result = !runtime::IsTrue(arg);
        return runtime::ObjectHolder::Own(runtime::Bool{ result });
    }

    Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
        : BinaryOperation(std::move(lhs), std::move(rhs))
        , cmp_(std::move(cmp)){}

    ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
        auto lhs = lhs_->Execute(closure, context);
        auto rhs = rhs_->Execute(closure, context);
        bool result = cmp_(lhs, rhs, context);
        return runtime::ObjectHolder::Own(runtime::Bool{ result });
    }

    NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args) 
        : cls_(class_)
        , args_(std::move(args)){}

    NewInstance::NewInstance(const runtime::Class& class_) 
        : cls_(class_){}

    ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
        std::vector<runtime::ObjectHolder> actual_args;
        for (const auto& arg : args_) {
            actual_args.push_back(arg->Execute(closure, context));
        }
        if (cls_.HasMethod(INIT_METHOD, args_.size())) {
            cls_.Call(INIT_METHOD, actual_args, context);
        }
        return ObjectHolder::Share(cls_);
    }

    MethodBody::MethodBody(std::unique_ptr<Statement>&& body) : body_(std::move(body)){}

    ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
        try {
            body_->Execute(closure, context);
            return runtime::ObjectHolder::None();
        }
        catch (runtime::ObjectHolder& obj) {
            return obj;
        }
    }

}  // namespace ast
