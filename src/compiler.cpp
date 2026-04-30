/**
 * @file compiler.cpp
 * @brief AST -> Bytecode compiler implementation.
 *
 * Calling convention:
 *   - Arguments are pushed onto the stack first
 *   - Then the function is loaded via LOAD_NAME
 *   - CALL_FUNCTION(num_args) pops function + args, creates call frame
 *   - RETURN_VALUE pops the return value, restores frame, pushes result
 */
#include "compiler.h"
#include <stdexcept>

namespace mimo {

uint32_t CompiledCode::add_constant(PyValue val) {
    constants.push_back(std::move(val));
    return static_cast<uint32_t>(constants.size() - 1);
}

uint32_t CompiledCode::add_name(const std::string& name) {
    names.push_back(name);
    return static_cast<uint32_t>(names.size() - 1);
}

uint32_t CompiledCode::find_or_add_name(const std::string& name) {
    for (uint32_t i = 0; i < names.size(); ++i) {
        if (names[i] == name) return i;
    }
    return add_name(name);
}

uint32_t CompiledCode::emit(Instruction instr) {
    uint32_t addr = current_address();
    instructions.push_back(instr);
    return addr;
}

uint32_t CompiledCode::current_address() const {
    return static_cast<uint32_t>(instructions.size());
}

Compiler::Compiler() = default;

CompiledCode Compiler::compile(ProgramNode& program) {
    code_ = CompiledCode{};
    for (auto& stmt : program.statements) {
        stmt->accept(*this);
    }
    code_.emit(Instruction(OpCode::HALT));
    return code_;
}

uint32_t Compiler::get_or_create_local_slot(const std::string& name) {
    auto it = local_slot_map_.find(name);
    if (it != local_slot_map_.end()) return it->second;
    uint32_t slot = static_cast<uint32_t>(local_slots_.size());
    local_slots_.push_back(name);
    local_slot_map_[name] = slot;
    return slot;
}

void Compiler::compile_statements(const std::vector<StmtPtr>& stmts) {
    for (auto& stmt : stmts) {
        stmt->accept(*this);
    }
}

// === Expression visitors ===

void Compiler::visit(IntLiteral& n) {
    uint32_t idx = code_.add_constant(PyValue(n.value));
    code_.emit(Instruction(OpCode::LOAD_CONST, idx));
}

void Compiler::visit(FloatLiteral& n) {
    uint32_t idx = code_.add_constant(PyValue(n.value));
    code_.emit(Instruction(OpCode::LOAD_CONST, idx));
}

void Compiler::visit(StringLiteral& n) {
    uint32_t idx = code_.add_constant(PyValue(n.value));
    code_.emit(Instruction(OpCode::LOAD_CONST, idx));
}

void Compiler::visit(BoolLiteral& n) {
    uint32_t idx = code_.add_constant(PyValue(n.value));
    code_.emit(Instruction(OpCode::LOAD_CONST, idx));
}

void Compiler::visit(NoneLiteral& /*n*/) {
    uint32_t idx = code_.add_constant(PyValue::none());
    code_.emit(Instruction(OpCode::LOAD_CONST, idx));
}

void Compiler::visit(Identifier& n) {
    if (in_function_ && local_slot_map_.count(n.name)) {
        code_.emit(Instruction(OpCode::LOAD_FAST, local_slot_map_[n.name]));
    } else {
        uint32_t idx = code_.find_or_add_name(n.name);
        code_.emit(Instruction(OpCode::LOAD_NAME, idx));
    }
}

void Compiler::visit(BinaryExpr& n) {
    if (n.op == TokenType::AND) {
        n.left->accept(*this);
        uint32_t jump_addr = code_.emit(Instruction(OpCode::JUMP_IF_FALSE, 0));
        code_.emit(Instruction(OpCode::POP_TOP));
        n.right->accept(*this);
        code_.instructions[jump_addr].operand = code_.current_address();
        return;
    }
    if (n.op == TokenType::OR) {
        n.left->accept(*this);
        code_.emit(Instruction(OpCode::DUP_TOP));
        uint32_t jump_addr = code_.emit(Instruction(OpCode::JUMP_IF_TRUE, 0));
        code_.emit(Instruction(OpCode::POP_TOP));
        n.right->accept(*this);
        code_.instructions[jump_addr].operand = code_.current_address();
        return;
    }

    n.left->accept(*this);
    n.right->accept(*this);

    switch (n.op) {
        case TokenType::PLUS:  code_.emit(Instruction(OpCode::BINARY_ADD)); break;
        case TokenType::MINUS: code_.emit(Instruction(OpCode::BINARY_SUB)); break;
        case TokenType::STAR:  code_.emit(Instruction(OpCode::BINARY_MUL)); break;
        case TokenType::SLASHSLASH: code_.emit(Instruction(OpCode::BINARY_FLOOR_DIV)); break;
        case TokenType::PERCENT:    code_.emit(Instruction(OpCode::BINARY_MOD)); break;
        case TokenType::EQ:    code_.emit(Instruction(OpCode::COMPARE_EQ)); break;
        case TokenType::NEQ:   code_.emit(Instruction(OpCode::COMPARE_NEQ)); break;
        case TokenType::LT:    code_.emit(Instruction(OpCode::COMPARE_LT)); break;
        case TokenType::GT:    code_.emit(Instruction(OpCode::COMPARE_GT)); break;
        case TokenType::LTE:   code_.emit(Instruction(OpCode::COMPARE_LTE)); break;
        case TokenType::GTE:   code_.emit(Instruction(OpCode::COMPARE_GTE)); break;
        default:
            throw std::runtime_error("Compiler: unsupported binary operator");
    }
}

void Compiler::visit(UnaryExpr& n) {
    n.operand->accept(*this);
    if (n.op == TokenType::MINUS) {
        code_.emit(Instruction(OpCode::UNARY_NEG));
    } else if (n.op == TokenType::NOT) {
        code_.emit(Instruction(OpCode::UNARY_NOT));
    }
}

void Compiler::visit(CallExpr& n) {
    // Push arguments first
    for (auto& arg : n.args) {
        arg->accept(*this);
    }
    // Load function
    uint32_t name_idx = code_.find_or_add_name(n.callee);
    code_.emit(Instruction(OpCode::LOAD_NAME, name_idx));
    // Call
    code_.emit(Instruction(OpCode::CALL_FUNCTION, static_cast<uint32_t>(n.args.size())));
}

// === Statement visitors ===

void Compiler::visit(ExprStmt& n) {
    n.expr->accept(*this);
    code_.emit(Instruction(OpCode::POP_TOP));
}

void Compiler::visit(AssignStmt& n) {
    n.value->accept(*this);
    if (in_function_) {
        uint32_t slot = get_or_create_local_slot(n.name);
        code_.emit(Instruction(OpCode::STORE_FAST, slot));
    } else {
        uint32_t idx = code_.find_or_add_name(n.name);
        code_.emit(Instruction(OpCode::STORE_NAME, idx));
    }
}

void Compiler::visit(PrintStmt& n) {
    for (auto& arg : n.args) {
        arg->accept(*this);
    }
    code_.emit(Instruction(OpCode::PRINT, static_cast<uint32_t>(n.args.size())));
}

void Compiler::visit(ReturnStmt& n) {
    if (n.value) {
        n.value->accept(*this);
    } else {
        uint32_t idx = code_.add_constant(PyValue::none());
        code_.emit(Instruction(OpCode::LOAD_CONST, idx));
    }
    code_.emit(Instruction(OpCode::RETURN_VALUE));
}

void Compiler::visit(IfStmt& n) {
    std::vector<uint32_t> end_jumps;

    for (size_t i = 0; i < n.branches.size(); ++i) {
        auto& branch = n.branches[i];

        if (branch.condition) {
            branch.condition->accept(*this);
            uint32_t false_jump = code_.emit(Instruction(OpCode::JUMP_IF_FALSE, 0));
            code_.emit(Instruction(OpCode::POP_TOP));  // pop condition (true path)

            compile_statements(branch.body);

            // Jump to end after executing this branch (skip else/elif)
            end_jumps.push_back(code_.emit(Instruction(OpCode::JUMP_ABSOLUTE, 0)));

            // False path: jump target
            code_.instructions[false_jump].operand = code_.current_address();
            code_.emit(Instruction(OpCode::POP_TOP));  // pop condition (false path)
        } else {
            compile_statements(branch.body);
        }
    }

    uint32_t end_addr = code_.current_address();
    for (auto addr : end_jumps) {
        code_.instructions[addr].operand = end_addr;
    }
}

void Compiler::visit(WhileStmt& n) {
    uint32_t loop_start = code_.current_address();

    LoopContext ctx;
    ctx.continue_target = loop_start;
    ctx.break_target = 0;
    loop_stack_.push_back(ctx);

    n.condition->accept(*this);
    uint32_t exit_jump = code_.emit(Instruction(OpCode::JUMP_IF_FALSE, 0));
    code_.emit(Instruction(OpCode::POP_TOP));

    compile_statements(n.body);

    code_.emit(Instruction(OpCode::JUMP_ABSOLUTE, loop_start));

    code_.instructions[exit_jump].operand = code_.current_address();
    code_.emit(Instruction(OpCode::POP_TOP));

    auto& finished_ctx = loop_stack_.back();
    uint32_t end_addr = code_.current_address();
    for (auto addr : finished_ctx.break_patches) {
        code_.instructions[addr].operand = end_addr;
    }
    for (auto addr : finished_ctx.continue_patches) {
        code_.instructions[addr].operand = finished_ctx.continue_target;
    }
    loop_stack_.pop_back();
}

void Compiler::visit(ForStmt& n) {
    auto* call = dynamic_cast<CallExpr*>(n.iterable.get());
    if (!call || call->callee != "range") {
        throw std::runtime_error("Compiler: for-in only supports range()");
    }

    if (call->args.size() == 1) {
        // range(end): i goes from 0 to end-1
        call->args[0]->accept(*this);
        uint32_t end_idx = code_.find_or_add_name("__range_end_" + n.var);
        code_.emit(Instruction(OpCode::STORE_NAME, end_idx));

        uint32_t zero_idx = code_.add_constant(PyValue(static_cast<int64_t>(0)));
        code_.emit(Instruction(OpCode::LOAD_CONST, zero_idx));
        uint32_t iter_idx = code_.find_or_add_name(n.var);
        code_.emit(Instruction(OpCode::STORE_NAME, iter_idx));

        uint32_t loop_start = code_.current_address();

        code_.emit(Instruction(OpCode::LOAD_NAME, iter_idx));
        code_.emit(Instruction(OpCode::LOAD_NAME, end_idx));
        code_.emit(Instruction(OpCode::COMPARE_LT));
        uint32_t exit_jump = code_.emit(Instruction(OpCode::JUMP_IF_FALSE, 0));
        code_.emit(Instruction(OpCode::POP_TOP));

        LoopContext ctx;
        ctx.continue_target = 0; // will be set after body
        ctx.break_target = 0;
        loop_stack_.push_back(ctx);

        compile_statements(n.body);

        // Increment section
        uint32_t incr_start = code_.current_address();
        code_.emit(Instruction(OpCode::LOAD_NAME, iter_idx));
        uint32_t one_idx = code_.add_constant(PyValue(static_cast<int64_t>(1)));
        code_.emit(Instruction(OpCode::LOAD_CONST, one_idx));
        code_.emit(Instruction(OpCode::BINARY_ADD));
        code_.emit(Instruction(OpCode::STORE_NAME, iter_idx));

        code_.emit(Instruction(OpCode::JUMP_ABSOLUTE, loop_start));

        code_.instructions[exit_jump].operand = code_.current_address();
        code_.emit(Instruction(OpCode::POP_TOP));

        // Patch break/continue
        auto& finished_ctx = loop_stack_.back();
        finished_ctx.continue_target = incr_start;
        uint32_t end_addr = code_.current_address();
        for (auto addr : finished_ctx.break_patches) {
            code_.instructions[addr].operand = end_addr;
        }
        for (auto addr : finished_ctx.continue_patches) {
            code_.instructions[addr].operand = finished_ctx.continue_target;
        }
        loop_stack_.pop_back();
    } else if (call->args.size() == 2) {
        // range(start, end)
        call->args[0]->accept(*this);
        uint32_t iter_idx = code_.find_or_add_name(n.var);
        code_.emit(Instruction(OpCode::STORE_NAME, iter_idx));

        call->args[1]->accept(*this);
        uint32_t end_idx = code_.find_or_add_name("__range_end_" + n.var);
        code_.emit(Instruction(OpCode::STORE_NAME, end_idx));

        uint32_t loop_start = code_.current_address();

        code_.emit(Instruction(OpCode::LOAD_NAME, iter_idx));
        code_.emit(Instruction(OpCode::LOAD_NAME, end_idx));
        code_.emit(Instruction(OpCode::COMPARE_LT));
        uint32_t exit_jump = code_.emit(Instruction(OpCode::JUMP_IF_FALSE, 0));
        code_.emit(Instruction(OpCode::POP_TOP));

        LoopContext ctx;
        ctx.continue_target = 0;
        ctx.break_target = 0;
        loop_stack_.push_back(ctx);

        compile_statements(n.body);

        uint32_t incr_start = code_.current_address();
        code_.emit(Instruction(OpCode::LOAD_NAME, iter_idx));
        uint32_t one_idx = code_.add_constant(PyValue(static_cast<int64_t>(1)));
        code_.emit(Instruction(OpCode::LOAD_CONST, one_idx));
        code_.emit(Instruction(OpCode::BINARY_ADD));
        code_.emit(Instruction(OpCode::STORE_NAME, iter_idx));

        code_.emit(Instruction(OpCode::JUMP_ABSOLUTE, loop_start));

        code_.instructions[exit_jump].operand = code_.current_address();
        code_.emit(Instruction(OpCode::POP_TOP));

        auto& finished_ctx = loop_stack_.back();
        finished_ctx.continue_target = incr_start;
        uint32_t end_addr = code_.current_address();
        for (auto addr : finished_ctx.break_patches) {
            code_.instructions[addr].operand = end_addr;
        }
        for (auto addr : finished_ctx.continue_patches) {
            code_.instructions[addr].operand = finished_ctx.continue_target;
        }
        loop_stack_.pop_back();
    } else {
        throw std::runtime_error("Compiler: range() supports 1 or 2 arguments");
    }
}

void Compiler::visit(FuncDef& n) {
    // Skip over function body at runtime
    uint32_t skip_jump = code_.emit(Instruction(OpCode::JUMP_ABSOLUTE, 0));

    uint32_t func_start = code_.current_address();

    // Save and set function-local context
    bool prev_in_function = in_function_;
    auto prev_local_slots = local_slots_;
    auto prev_local_slot_map = local_slot_map_;
    in_function_ = true;
    local_slots_.clear();
    local_slot_map_.clear();

    // Register parameters as local slots
    for (auto& param : n.params) {
        get_or_create_local_slot(param);
    }

    compile_statements(n.body);

    // Implicit return None if no explicit return
    uint32_t none_idx = code_.add_constant(PyValue::none());
    code_.emit(Instruction(OpCode::LOAD_CONST, none_idx));
    code_.emit(Instruction(OpCode::RETURN_VALUE));

    // Save local slot info for the VM
    auto func_local_slots = local_slots_;

    // Restore previous context
    in_function_ = prev_in_function;
    local_slots_ = prev_local_slots;
    local_slot_map_ = prev_local_slot_map;

    // Patch skip jump
    code_.instructions[skip_jump].operand = code_.current_address();

    // Create function object and store it
    auto func = std::make_shared<PyFunction>();
    func->name = n.name;
    func->params = n.params;
    func->entry_point = func_start;
    // Store local slot names so VM can set up fast_locals
    func->local_slot_names = std::move(func_local_slots);

    uint32_t func_idx = code_.add_constant(PyValue(func));
    code_.emit(Instruction(OpCode::LOAD_CONST, func_idx));
    uint32_t name_idx = code_.find_or_add_name(n.name);
    code_.emit(Instruction(OpCode::STORE_NAME, name_idx));
}

void Compiler::visit(BreakStmt& n) {
    if (loop_stack_.empty()) {
        throw std::runtime_error("Compiler: 'break' outside of loop at line " + std::to_string(n.line));
    }
    uint32_t addr = code_.emit(Instruction(OpCode::JUMP_ABSOLUTE, 0));
    loop_stack_.back().break_patches.push_back(addr);
}

void Compiler::visit(ContinueStmt& n) {
    if (loop_stack_.empty()) {
        throw std::runtime_error("Compiler: 'continue' outside of loop at line " + std::to_string(n.line));
    }
    uint32_t addr = code_.emit(Instruction(OpCode::JUMP_ABSOLUTE, 0));
    loop_stack_.back().continue_patches.push_back(addr);
}

void Compiler::visit(PassStmt& /*n*/) {
    // No-op
}

} // namespace mimo
