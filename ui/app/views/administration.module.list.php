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
 * @var array $data
 */

if ($data['uncheck']) {
	uncheckTableRows('modules');
}

$this->includeJsFile('administration.module.list.js.php');

$html_page = (new CHtmlPage())
	->setTitle(_('Modules'))
	->setTitleSubmenu(getAdministrationGeneralSubmenu())
	->setDocUrl(CDocHelper::getUrl(CDocHelper::ADMINISTRATION_MODULE_LIST))
	->setControls(
		(new CTag('nav', true,
			(new CForm())
				->addCsrfToken('module.scan')
				->addVar('action', 'module.scan')
				->addItem((new CList())
					->addItem(new CSubmit('form', _('Scan directory')))
				)
		))->setAttribute('aria-label', _('Content controls'))
	)
	->addItem(
		(new CFilter())
			->setResetUrl((new CUrl('zabbix.php'))->setArgument('action', 'module.list'))
			->addVar('action', 'module.list')
			->setProfile($data['filter_profile'])
			->setActiveTab($data['filter_active_tab'])
			->addFilterTab(_('Filter'), [
				(new CFormList())->addRow(_('Name'),
					(new CTextBox('filter_name', $data['filter']['name']))
						->setWidth(ZBX_TEXTAREA_FILTER_SMALL_WIDTH)
						->setAttribute('autofocus', 'autofocus')
				),
				(new CFormList())->addRow(_('Status'),
					(new CRadioButtonList('filter_status', (int) $data['filter']['status']))
						->addValue(_('Any'), -1)
						->addValue(_('Enabled'), MODULE_STATUS_ENABLED)
						->addValue(_('Disabled'), MODULE_STATUS_DISABLED)
						->setModern(true)
				)
			])
	);

// create form
$form = (new CForm())->setName('module-form');

// create table
$table = (new CTableInfo())
	->setHeader([
		(new CColHeader(
			(new CCheckBox('all_modules'))
				->onClick("checkAll('".$form->getName()."', 'all_modules', 'moduleids');")
		))->addClass(ZBX_STYLE_CELL_WIDTH),
		make_sorting_header(_('Name'), 'name', $data['sort'], $data['sortorder'],
			(new CUrl('zabbix.php'))
				->setArgument('action', 'module.list')
				->getUrl()
		),
		_('Version'),
		_('Author'),
		_('Description'),
		_('Status')
	]);

foreach ($data['modules'] as $moduleid => $module) {
	$name = new CLink($module['name'],
		(new CUrl('zabbix.php'))
			->setArgument('action', 'module.edit')
			->setArgument('moduleid', $moduleid)
			->getUrl()
	);

	$status_url = (new CUrl('zabbix.php'))
		->setArgument('action', ($module['status'] == MODULE_STATUS_ENABLED) ? 'module.disable' : 'module.enable')
		->setArgument('moduleids[]', $moduleid)
		->getUrl();

	if ($module['status'] == MODULE_STATUS_ENABLED) {
		$status = (new CLink(_('Enabled'), (new CUrl($status_url))->setArgumentCsrfToken()->getUrl()))
			->addClass(ZBX_STYLE_LINK_ACTION)
			->addClass(ZBX_STYLE_GREEN);
	}
	else {
		$status = (new CLink(_('Disabled'), (new CUrl($status_url))->setArgumentCsrfToken()->getUrl()))
			->addClass(ZBX_STYLE_LINK_ACTION)
			->addClass(ZBX_STYLE_RED);
	}

	// append table row
	$table->addRow([
		new CCheckBox('moduleids['.$moduleid.']', $moduleid),
		(new CCol($name))->addClass(ZBX_STYLE_NOWRAP),
		$module['version'],
		$module['author'],
		$module['description'],
		$status
	]);
}

// append table to form
$form->addItem([
	$table,
	$data['paging'],
	new CActionButtonList('action', 'moduleids', [
		'module.enable' => [
			'content' => (new CSubmitButton(_('Enable'), 'action', 'module.enable'))
				->addClass(ZBX_STYLE_BTN_ALT)
				->addClass('js-massenable-module')
				->addClass('no-chkbxrange')
				->removeid()
		],
		'module.disable' => [
			'content' => (new CSubmitButton(_('Disable'), 'action', 'module.disable'))
				->addClass(ZBX_STYLE_BTN_ALT)
				->addClass('js-massdisable-module')
				->addClass('no-chkbxrange')
				->removeid()
		]
	], 'modules')
]);

// append form to widget
$html_page->addItem($form);

$html_page->show();

(new CScriptTag('view.init('.json_encode([
		'csrf_tokens' => $data['csrf_tokens']
	]).');'))
	->setOnDocumentReady()
	->show();
