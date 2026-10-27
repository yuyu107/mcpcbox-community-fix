#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
mcpc_test_dir="$(mktemp -d)"
trap 'rm -rf "$mcpc_test_dir"' EXIT
node native-v19/tests/test_ui.js
node native-v19/tests/test_history_ui.js
gcc -std=c99 -Wall -Wextra -Werror native-v19/tests/test_core.c native-v19/core.c -o "$mcpc_test_dir/core"
"$mcpc_test_dir/core"
gcc -std=c99 -Wall -Wextra -Werror -Wno-misleading-indentation native-v19/tests/test_first_language.c native-v19/core.c -o "$mcpc_test_dir/language"
"$mcpc_test_dir/language"
gcc -std=c99 -Wall -Wextra -Werror -Wno-misleading-indentation native-v19/tests/test_cache.c -lcrypto -lpthread -o "$mcpc_test_dir/cache"
"$mcpc_test_dir/cache"
gcc -std=c99 -Wall -Wextra -Werror -Wno-misleading-indentation native-v19/tests/test_history.c -o "$mcpc_test_dir/history"
"$mcpc_test_dir/history"
gcc -std=c99 -Wall -Wextra -Werror -Wno-misleading-indentation native-v19/tests/test_history_delete.c -o "$mcpc_test_dir/history-delete"
"$mcpc_test_dir/history-delete"

gcc -std=c99 -Wall -Wextra -Werror -Wno-misleading-indentation native-v19/tests/test_history_store.c native-v19/core.c -lpthread -o "$mcpc_test_dir/history-store"
"$mcpc_test_dir/history-store"
