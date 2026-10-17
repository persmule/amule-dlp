// Fetch and freeze the identity shown in the confirmation, then send that exact
// target even if SSE updates the table or detail panel while the dialog is open.
import { api } from "./api.js";
import { data } from "./events.js";
import { confirmDialog, toast } from "./components.js";
import { t, terr } from "./i18n.js";
import { prepareSharedDeletion } from "./download-delete.js";

export async function deleteSharedFile(hash) {
  try {
    const prepared = await prepareSharedDeletion([hash], (h) => api.get("shared/" + h));
    const file = prepared.files[0];
    if (file.incomplete) {
      toast(t("shared_delete_incomplete"), "error");
      return;
    }
    if (!(await confirmDialog(t("downloads_confirm_delete_disk", file)))) return;
    await api.del("shared/" + file.hash, { confirmed_target: prepared.confirmed_target[file.hash] });
    data.refresh("shared");
    data.refresh("downloads");
  } catch (e) { toast(terr(e), "error"); }
}
