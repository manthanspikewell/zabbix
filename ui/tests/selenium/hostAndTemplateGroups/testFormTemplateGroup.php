<?php
/*
** Zabbix
** Copyright (C) 2001-2023 Zabbix SIA
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


require_once dirname(__FILE__).'/../common/testFormGroups.php';

/**
 * @backup hosts
 *
 * @onBefore prepareGroupData
 */
class testFormTemplateGroup extends testFormGroups {

	protected $link = 'zabbix.php?action=templategroup.list';
	protected $object = 'template';
	protected static $update_group = 'Group for Update test';

	public function testFormTemplateGroup_Layout() {
		$this->layout('Templates');
	}

	public static function getTemplateCreateData() {
		return [
			[
				[
					'expected' => TEST_GOOD,
					'fields' => [
						'Group name' => 'Zabbix servers'
					]
				]
			],
			[
				[
					'expected' => TEST_BAD,
					'fields' => [
						'Group name' => 'Templates'
					],
					'error' => 'Template group "Templates" already exists.'
				]
			],
			[
				[
					'expected' => TEST_GOOD,
					'fields' => [
						'Group name' => STRING_255
					]
				]
			]
		];
	}

	/**
	 * @dataProvider getCreateData
	 * @dataProvider getTemplateCreateData
	 */
	public function testFormTemplateGroup_Create($data) {
		$this->checkForm($data, 'create');
	}

	public static function getTemplateUpdateData() {
		return [
			[
				[
					'expected' => TEST_GOOD,
					'fields' => [
						'Group name' => 'Discovered hosts'
					]
				]
			],
			[
				[
					'expected' => TEST_BAD,
					'fields' => [
						'Group name' => 'Templates',
						'Apply permissions to all subgroups' => true
					],
					'error' => 'Template group "Templates" already exists.'
				]
			],
			[
				[
					'expected' => TEST_GOOD,
					'fields' => [
						'Group name' => str_repeat('long_', 51)
					]
				]
			]
		];
	}

	/**
	 * @dataProvider getUpdateData
	 * @dataProvider getTemplateUpdateData
	 */
	public function testFormTemplateGroup_Update($data) {
		$this->checkForm($data, 'update');
	}

	/**
	 * Test group simple update without changing data.
	 */
	public function testFormTemplateGroup_SimpleUpdate() {
		$this->simpleUpdate('Templates');
	}

	/**
	 * @dataProvider getCloneData
	 */
	public function testFormTemplateGroup_Clone($data) {
		$this->clone($data);
	}

	/**
	 * @dataProvider getCancelData
	 */
	public function testFormTemplateGroup_Cancel($data) {
		$this->cancel($data);
	}

	public static function getTemplateDeleteData() {
		return [
			[
				[
					'expected' => TEST_BAD,
					'name' => 'One group for Delete',
					'error' => 'Template "Template for group testing" cannot be without template group.'
				]
			]
		];
	}

	/**
	 * @dataProvider getDeleteData
	 * @dataProvider getTemplateDeleteData
	 */
	public function testFormTemplateGroup_Delete($data) {
		$this->delete($data);
	}

	/**
	 * @onBeforeOnce prepareSubgroupData
	 * @dataProvider getSubgroupsData
	 */
	public function testFormTemplateGroup_ApplyPermissionsToSubgroups($data) {
		$this->checkSubgroupsPermissions($data);
	}
}
