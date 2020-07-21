/*
 * Copyright (c) 2019 Broadcom.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *   Broadcom, Inc. - initial API and implementation
 */

#ifndef HLASMPLUGIN_PARSERLIBRARY_FILE_CACHE_CONTEXT_H
#define HLASMPLUGIN_PARSERLIBRARY_FILE_CACHE_CONTEXT_H

#include <memory>

namespace hlasm_plugin::parser_library::workspaces {

// abstract class that represent a file context that can be cached between parsings
class file_cache_context
{
public:
    virtual ~file_cache_context() = default;
};

using file_cache_ctx_ptr = std::shared_ptr<file_cache_context>;

} // namespace hlasm_plugin::parser_library::workspaces

#endif
