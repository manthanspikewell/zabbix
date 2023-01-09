<?php
/*
** Zabbix
** Copyright (C) 2001-2022 Zabbix SIA
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**/


/**
 * @var CView $this
 */
?>

<script type="text/x-jquery-tmpl" id="filter-tag-row-tmpl">
	<?= CTagFilterFieldHelper::getTemplate() ?>
</script>

<script>
	const view = {
		checkbox_object: null,
		checkbox_hash: null,
		csrf_tokens: null,
		compression_status: null,

		init({checkbox_hash, checkbox_object, csrf_tokens, compression_status}) {
			this.checkbox_hash = checkbox_hash;
			this.checkbox_object = checkbox_object;
			this.csrf_tokens = csrf_tokens;
			this.compression_status = compression_status;

			this._initActionButtons();

			// Disable the status filter when using the state filter.
			$('#filter_state')
				.on('change', function() {
					$('input[name=filter_status]').prop('disabled', $('input[name=filter_state]:checked').val() != -1);
				})
				.trigger('change');

			$('#filter-tags')
				.dynamicRows({template: '#filter-tag-row-tmpl'})
				.on('afteradd.dynamicRows', function() {
					const rows = this.querySelectorAll('.form_row');
					new CTagFilterItem(rows[rows.length - 1]);
				});

			// Init existing fields once loaded.
			document.querySelectorAll('#filter-tags .form_row').forEach(row => {
				new CTagFilterItem(row);
			});
		},

		_initActionButtons() {
			document.addEventListener('click', (e) => {
				let prevent_event = false;

				if (e.target.classList.contains('js-massenable-item')) {
					prevent_event = !this.massEnableItem(e.target, Object.keys(chkbxRange.getSelectedIds()));
				}
				else if (e.target.classList.contains('js-massdisable-item')) {
					prevent_event = !this.massDisableItem(e.target, Object.keys(chkbxRange.getSelectedIds()));
				}
				else if (e.target.classList.contains('js-massclearhistory-item')) {
					prevent_event = !this.massClearHistoryItem(e.target, Object.keys(chkbxRange.getSelectedIds()));
				}
				else if (e.target.classList.contains('js-masscopyto-item')) {
					this.massCopytoItem(e.target, Object.keys(chkbxRange.getSelectedIds()));
				}
				else if (e.target.classList.contains('js-massdelete-item')) {
					prevent_event = !this.massDeleteItem(e.target, Object.keys(chkbxRange.getSelectedIds()));
				}

				if (prevent_event) {
					e.preventDefault();
					e.stopPropagation();
					return false;
				}
			});
		},

		massEnableItem(target, itemids) {
			const confirmation = itemids.length > 1
				? <?= json_encode(_('Enable selected items?')) ?>
				: <?= json_encode(_('Enable selected item?')) ?>;

			if (!window.confirm(confirmation)) {
				return false;
			}

			create_var(target.closest('form'), '<?= CController::CSRF_TOKEN_NAME ?>',
				this.csrf_tokens['item.massenable'], false
			);

			return true;
		},

		massDisableItem(target, itemids) {
			const confirmation = itemids.length > 1
				? <?= json_encode(_('Disable selected items?')) ?>
				: <?= json_encode(_('Disable selected item?')) ?>;

			if (!window.confirm(confirmation)) {
				return false;
			}

			create_var(target.closest('form'), '<?= CController::CSRF_TOKEN_NAME ?>',
				this.csrf_tokens['item.massdisable'], false
			);

			return true;
		},

		massClearHistoryItem(target, itemids) {
			if (this.compression_status) {
				const confirmation = itemids.length > 1
					? <?= json_encode(_('Delete history of selected items?')) ?>
					: <?= json_encode(_('Delete history of selected item?')) ?>;

				if (!window.confirm(confirmation)) {
					return false;
				}
			}

			create_var(target.closest('form'), '<?= CController::CSRF_TOKEN_NAME ?>',
				this.csrf_tokens['item.massclearhistory'], false
			);

			return true;
		},

		massCopytoItem(target) {
			create_var(target.closest('form'), '<?= CController::CSRF_TOKEN_NAME ?>',
				this.csrf_tokens['item.masscopyto'], false
			);
		},

		massDeleteItem(target, itemids) {
			const confirmation = itemids.length > 1
				? <?= json_encode(_('Delete selected items?')) ?>
				: <?= json_encode(_('Delete selected item?')) ?>;

			if (!window.confirm(confirmation)) {
				return false;
			}

			create_var(target.closest('form'), '<?= CController::CSRF_TOKEN_NAME ?>',
				this.csrf_tokens['item.massdelete'], false
			);

			return true;
		},

		editHost(e, hostid) {
			e.preventDefault();
			const host_data = {hostid};

			this.openHostPopup(host_data);
		},

		openHostPopup(host_data) {
			const original_url = location.href;
			const overlay = PopUp('popup.host.edit', host_data, {
				dialogueid: 'host_edit',
				dialogue_class: 'modal-popup-large',
				prevent_navigation: true
			});

			overlay.$dialogue[0].addEventListener('dialogue.create', this.events.hostSuccess, {once: true});
			overlay.$dialogue[0].addEventListener('dialogue.update', this.events.hostSuccess, {once: true});
			overlay.$dialogue[0].addEventListener('dialogue.delete', this.events.hostDelete, {once: true});
			overlay.$dialogue[0].addEventListener('overlay.close', () => {
				history.replaceState({}, '', original_url);
			}, {once: true});
		},

		massCheckNow(button) {
			button.classList.add('is-loading');

			const curl = new Curl('zabbix.php');
			curl.setAction('item.masscheck_now', this.csrf_tokens['item.masscheck_now']);

			fetch(curl.getUrl(), {
				method: 'POST',
				headers: {'Content-Type': 'application/json'},
				body: JSON.stringify({itemids: Object.keys(chkbxRange.getSelectedIds())})
			})
				.then((response) => response.json())
				.then((response) => {
					clearMessages();

					if ('error' in response) {
						addMessage(makeMessageBox('bad', [response.error.messages], response.error.title, true, true));
					}
					else if('success' in response) {
						addMessage(makeMessageBox('good', [], response.success.title, true, false));

						const uncheckids = Object.keys(chkbxRange.getSelectedIds());
						uncheckTableRows('items_' + this.checkbox_hash, [], false);
						chkbxRange.checkObjects(this.checkbox_object, uncheckids, false);
						chkbxRange.update(this.checkbox_object);
					}
				})
				.catch(() => {
					const title = <?= json_encode(_('Unexpected server error.')) ?>;
					const message_box = makeMessageBox('bad', [], title)[0];

					clearMessages();
					addMessage(message_box);
				})
				.finally(() => {
					button.classList.remove('is-loading');

					// Deselect the "Execute now" button in both success and error cases, since there is no page reload.
					button.blur();
				});
		},

		checkNow(itemid) {
			const curl = new Curl('zabbix.php');
			curl.setAction('item.masscheck_now', this.csrf_tokens['item.masscheck_now']);

			fetch(curl.getUrl(), {
				method: 'POST',
				headers: {'Content-Type': 'application/json'},
				body: JSON.stringify({itemids: [itemid]})
			})
				.then((response) => response.json())
				.then((response) => {
					clearMessages();

					/*
					 * Using postMessageError or postMessageOk would mean that those messages are stored in session
					 * messages and that would mean to reload the page and show them. Also postMessageError would be
					 * displayed right after header is loaded. Meaning message is not inside the page form like that is
					 * in postMessageOk case. Instead show message directly that comes from controller.
					 */
					if ('error' in response) {
						addMessage(makeMessageBox('bad', [response.error.messages], response.error.title, true, true));
					}
					else if('success' in response) {
						addMessage(makeMessageBox('good', [], response.success.title, true, false));
					}
				})
				.catch(() => {
					const title = <?= json_encode(_('Unexpected server error.')) ?>;
					const message_box = makeMessageBox('bad', [], title)[0];

					clearMessages();
					addMessage(message_box);
				});
		},

		events: {
			hostSuccess(e) {
				const data = e.detail;

				if ('success' in data) {
					postMessageOk(data.success.title);

					if ('messages' in data.success) {
						postMessageDetails('success', data.success.messages);
					}
				}

				location.href = location.href;
			},

			hostDelete(e) {
				const data = e.detail;

				if ('success' in data) {
					postMessageOk(data.success.title);

					if ('messages' in data.success) {
						postMessageDetails('success', data.success.messages);
					}
				}

				const curl = new Curl('zabbix.php');
				curl.setArgument('action', 'host.list');

				location.href = curl.getUrl();
			}
		}
	};
</script>
