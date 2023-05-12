// Copyright 2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_I18N_HPP
#define PATCHAGE_I18N_HPP

#include <libintl.h>

/// Mark a string literal as translatable
#define T(msgid) gettext(msgid)

#endif // PATCHAGE_I18N_HPP
